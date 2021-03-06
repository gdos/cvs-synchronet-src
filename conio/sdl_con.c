#if (defined(__MACH__) && defined(__APPLE__))
#include <Carbon/Carbon.h>
#define USE_PASTEBOARD
#include "pasteboard.h"
#endif

#include <stdarg.h>
#include <stdio.h>		/* NULL */
#include <stdlib.h>
#include <string.h>

#include "gen_defs.h"
#include "genwrap.h"
#include "dirwrap.h"
#include "xpbeep.h"
#include "threadwrap.h"
#ifdef __unix__
#include <xp_dl.h>
#endif

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"
#include "vidmodes.h"
#define BITMAP_CIOLIB_DRIVER
#include "bitmap_con.h"

#include "SDL.h"
#include "SDL_thread.h"

#include "sdlfuncs.h"

int bitmap_width,bitmap_height;

/* 256 bytes so I can cheat */
unsigned char		sdl_keybuf[256];		/* Keyboard buffer */
unsigned char		sdl_key=0;				/* Index into keybuf for next key in buffer */
unsigned char		sdl_keynext=0;			/* Index into keybuf for next free position */

int sdl_exitcode=0;

SDL_Surface	*win=NULL;
SDL_mutex	*win_mutex;
SDL_Surface	*sdl_icon=NULL;
SDL_Surface	*new_rect=NULL;
SDL_mutex	*newrect_mutex;
SDL_mutex	*bitmap_init_mutex;
static int bitmap_initialized = 0;

/* *nix copy/paste stuff */
int paste_needs_events;
int copy_needs_events;
SDL_sem	*sdl_pastebuf_set;
SDL_sem	*sdl_pastebuf_copied;
SDL_mutex	*sdl_copybuf_mutex;
static SDL_Thread *mouse_thread;
char *sdl_copybuf=NULL;
char *sdl_pastebuf=NULL;

SDL_sem *sdl_ufunc_ret;
SDL_sem *sdl_ufunc_rec;
SDL_mutex *sdl_ufunc_mtx;
int sdl_ufunc_retval;

SDL_sem	*sdl_flush_sem;
int pending_updates=0;

int fullscreen=0;

int	sdl_init_good=0;
SDL_mutex *sdl_keylock;
SDL_sem *sdl_key_pending;
static unsigned int sdl_pending_mousekeys=0;
static int sdl_using_directx=0;
static int sdl_using_quartz=0;
static int sdl_using_x11=0;
static int sdl_x11available=0;

static struct video_stats cvstat;

struct yuv_settings {
	int			enabled;
	int			win_width;
	int			win_height;
	int			screen_width;
	int			screen_height;
	int			changed;
	int			best_format;
	SDL_Overlay	*overlay;
};

static struct yuv_settings yuv={0,0,0,0,0,0,0,NULL};

struct sdl_keyvals {
	int	keysym
		,key
		,shift
		,ctrl
		,alt;
};

static SDL_mutex *sdl_headlock;
static struct rectlist *update_list = NULL;
static struct rectlist *update_list_tail = NULL;

enum {
	 SDL_USEREVENT_FLUSH
	,SDL_USEREVENT_SETTITLE
	,SDL_USEREVENT_SETNAME
	,SDL_USEREVENT_SETICON
	,SDL_USEREVENT_SETVIDMODE
	,SDL_USEREVENT_SHOWMOUSE
	,SDL_USEREVENT_HIDEMOUSE
	,SDL_USEREVENT_INIT
	,SDL_USEREVENT_COPY
	,SDL_USEREVENT_PASTE
	,SDL_USEREVENT_QUIT
};

const struct sdl_keyvals sdl_keyval[] =
{
	{SDLK_BACKSPACE, 0x08, 0x08, 0x7f, 0x0e00},
	{SDLK_TAB, 0x09, 0x0f00, 0x9400, 0xa500},
	{SDLK_RETURN, 0x0d, 0x0d, 0x0a, 0xa600},
	{SDLK_ESCAPE, 0x1b, 0x1b, 0x1b, 0x0100},
	{SDLK_SPACE, 0x20, 0x20, 0x0300, 0x20},
	{SDLK_0, '0', ')', 0, 0x8100},
	{SDLK_1, '1', '!', 0, 0x7800},
	{SDLK_2, '2', '@', 0x0300, 0x7900},
	{SDLK_3, '3', '#', 0, 0x7a00},
	{SDLK_4, '4', '$', 0, 0x7b00},
	{SDLK_5, '5', '%', 0, 0x7c00},
	{SDLK_6, '6', '^', 0x1e, 0x7d00},
	{SDLK_7, '7', '&', 0, 0x7e00},
	{SDLK_8, '8', '*', 0, 0x7f00},
	{SDLK_9, '9', '(', 0, 0x8000},
	{SDLK_a, 'a', 'A', 0x01, 0x1e00},
	{SDLK_b, 'b', 'B', 0x02, 0x3000},
	{SDLK_c, 'c', 'C', 0x03, 0x2e00},
	{SDLK_d, 'd', 'D', 0x04, 0x2000},
	{SDLK_e, 'e', 'E', 0x05, 0x1200},
	{SDLK_f, 'f', 'F', 0x06, 0x2100},
	{SDLK_g, 'g', 'G', 0x07, 0x2200},
	{SDLK_h, 'h', 'H', 0x08, 0x2300},
	{SDLK_i, 'i', 'I', 0x09, 0x1700},
	{SDLK_j, 'j', 'J', 0x0a, 0x2400},
	{SDLK_k, 'k', 'K', 0x0b, 0x2500},
	{SDLK_l, 'l', 'L', 0x0c, 0x2600},
	{SDLK_m, 'm', 'M', 0x0d, 0x3200},
	{SDLK_n, 'n', 'N', 0x0e, 0x3100},
	{SDLK_o, 'o', 'O', 0x0f, 0x1800},
	{SDLK_p, 'p', 'P', 0x10, 0x1900},
	{SDLK_q, 'q', 'Q', 0x11, 0x1000},
	{SDLK_r, 'r', 'R', 0x12, 0x1300},
	{SDLK_s, 's', 'S', 0x13, 0x1f00},
	{SDLK_t, 't', 'T', 0x14, 0x1400},
	{SDLK_u, 'u', 'U', 0x15, 0x1600},
	{SDLK_v, 'v', 'V', 0x16, 0x2f00},
	{SDLK_w, 'w', 'W', 0x17, 0x1100},
	{SDLK_x, 'x', 'X', 0x18, 0x2d00},
	{SDLK_y, 'y', 'Y', 0x19, 0x1500},
	{SDLK_z, 'z', 'Z', 0x1a, 0x2c00},
	{SDLK_PAGEUP, 0x4900, 0x4900, 0x8400, 0x9900},
	{SDLK_PAGEDOWN, 0x5100, 0x5100, 0x7600, 0xa100},
	{SDLK_END, 0x4f00, 0x4f00, 0x7500, 0x9f00},
	{SDLK_HOME, 0x4700, 0x4700, 0x7700, 0x9700},
	{SDLK_LEFT, 0x4b00, 0x4b00, 0x7300, 0x9b00},
	{SDLK_UP, 0x4800, 0x4800, 0x8d00, 0x9800},
	{SDLK_RIGHT, 0x4d00, 0x4d00, 0x7400, 0x9d00},
	{SDLK_DOWN, 0x5000, 0x5000, 0x9100, 0xa000},
	{SDLK_INSERT, 0x5200, 0x5200, 0x9200, 0xa200},
	{SDLK_DELETE, 0x5300, 0x5300, 0x9300, 0xa300},
	{SDLK_KP0, 0x5200, 0x5200, 0x9200, 0},
	{SDLK_KP1, 0x4f00, 0x4f00, 0x7500, 0},
	{SDLK_KP2, 0x5000, 0x5000, 0x9100, 0},
	{SDLK_KP3, 0x5100, 0x5100, 0x7600, 0},
	{SDLK_KP4, 0x4b00, 0x4b00, 0x7300, 0},
	{SDLK_KP5, 0x4c00, 0x4c00, 0x8f00, 0},
	{SDLK_KP6, 0x4d00, 0x4d00, 0x7400, 0},
	{SDLK_KP7, 0x4700, 0x4700, 0x7700, 0},
	{SDLK_KP8, 0x4800, 0x4800, 0x8d00, 0},
	{SDLK_KP9, 0x4900, 0x4900, 0x8400, 0},
	{SDLK_KP_MULTIPLY, '*', '*', 0x9600, 0x3700},
	{SDLK_KP_PLUS, '+', '+', 0x9000, 0x4e00},
	{SDLK_KP_MINUS, '-', '-', 0x8e00, 0x4a00},
	{SDLK_KP_PERIOD, 0x7f, 0x7f, 0x5300, 0x9300},
	{SDLK_KP_DIVIDE, '/', '/', 0x9500, 0xa400},
	{SDLK_KP_ENTER, 0x0d, 0x0d, 0x0a, 0xa600},
	{SDLK_F1, 0x3b00, 0x5400, 0x5e00, 0x6800},
	{SDLK_F2, 0x3c00, 0x5500, 0x5f00, 0x6900},
	{SDLK_F3, 0x3d00, 0x5600, 0x6000, 0x6a00},
	{SDLK_F4, 0x3e00, 0x5700, 0x6100, 0x6b00},
	{SDLK_F5, 0x3f00, 0x5800, 0x6200, 0x6c00},
	{SDLK_F6, 0x4000, 0x5900, 0x6300, 0x6d00},
	{SDLK_F7, 0x4100, 0x5a00, 0x6400, 0x6e00},
	{SDLK_F8, 0x4200, 0x5b00, 0x6500, 0x6f00},
	{SDLK_F9, 0x4300, 0x5c00, 0x6600, 0x7000},
	{SDLK_F10, 0x4400, 0x5d00, 0x6700, 0x7100},
	{SDLK_F11, 0x8500, 0x8700, 0x8900, 0x8b00},
	{SDLK_F12, 0x8600, 0x8800, 0x8a00, 0x8c00},
	{SDLK_BACKSLASH, '\\', '|', 0x1c, 0x2b00},
	{SDLK_SLASH, '/', '?', 0, 0x3500},
	{SDLK_MINUS, '-', '_', 0x1f, 0x8200},
	{SDLK_EQUALS, '=', '+', 0, 0x8300},
	{SDLK_LEFTBRACKET, '[', '{', 0x1b, 0x1a00},
	{SDLK_RIGHTBRACKET, ']', '}', 0x1d, 0x1b00},
	{SDLK_SEMICOLON, ';', ':', 0, 0x2700},
	{SDLK_QUOTE, '\'', '"', 0, 0x2800},
	{SDLK_COMMA, ',', '<', 0, 0x3300},
	{SDLK_PERIOD, '.', '>', 0, 0x3400},
	{SDLK_BACKQUOTE, '`', '~', 0, 0x2900},
	{0, 0, 0, 0, 0}	/** END **/
};

void sdl_setscaling(int new_value);

#if !defined(NO_X) && defined(__unix__)
#include "SDL_syswm.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#define CONSOLE_CLIPBOARD	XA_PRIMARY

/* X functions */
struct x11 {
	int		(*XFree)		(void *data);
	Window	(*XGetSelectionOwner)	(Display*, Atom);
	int		(*XConvertSelection)	(Display*, Atom, Atom, Atom, Window, Time);
	int		(*XGetWindowProperty)	(Display*, Window, Atom, long, long, Bool, Atom, Atom*, int*, unsigned long *, unsigned long *, unsigned char **);
	int		(*XChangeProperty)		(Display*, Window, Atom, Atom, int, int, _Xconst unsigned char*, int);
	Status	(*XSendEvent)	(Display*, Window, Bool, long, XEvent*);
	int		(*XSetSelectionOwner)	(Display*, Atom, Window, Time);
};
struct x11 sdl_x11;
#endif

static void RGBtoYUV(Uint8 r, Uint8 g, Uint8 b, Uint8 *yuv_array)
{
	int i;

	//yuv_array[0] = (Uint8)((0.257 * r) + (0.504 * g) + (0.098 * b) + 16);
	i = (r*263+g*516+b*100+16384);
	yuv_array[0] = i >> 10;
	//yuv_array[1] = (Uint8)(128 - (0.148 * r) - (0.291 * g) + (0.439 * b));
	i = 131072 - 152*r - 298*g + 450*b;
	yuv_array[1] = i >> 10;
	//yuv_array[2] = (Uint8)(128 + (0.439 * r) - (0.368 * g) - (0.071 * b));
	i = 131072 + 450*r - 377*g - 73*b;
	yuv_array[2] = i >> 10;
}

static void yuv_fillrect(SDL_Overlay *restrict overlay, SDL_Rect *restrict r, Uint8 *restrict yuvc)
{
	int uplane,vplane;					/* Planar formats */
	int y0pack, y1pack, u0pack, v0pack;	/* Packed formats */

	if(!overlay)
		return;
	if(r->x > overlay->w || r->y > overlay->h)
		return;
	if(r->x + r->w > overlay->w)
		r->w=overlay->w-r->x;
	if(r->y + r->h > overlay->h)
		r->h=overlay->h-r->y;
	yuv.changed=1;
	switch(overlay->format) {
		case SDL_IYUV_OVERLAY:
			/* YUV 4:2:0 NxM Y followed by (N/2)x(M/2) U and V (12bpp) */
			uplane=1;
			vplane=2;
			goto planar;
		case SDL_YV12_OVERLAY:
			/* YUV 4:2:0 NxM Y followed by (N/2)x(M/2) V and U (12bpp) */
			vplane=1;
			uplane=2;
			goto planar;
		case SDL_YUY2_OVERLAY:
			/* YUV 4:2:2 Y0,U0,Y1,V0 (16bpp) */
			y0pack=0;
			u0pack=1;
			y1pack=2;
			v0pack=3;
			goto packed;
		case SDL_UYVY_OVERLAY:
			/* YUV 4:2:2 U0,Y0,V0,Y1 (16bpp)  */
			u0pack=0;
			y0pack=1;
			v0pack=2;
			y1pack=3;
			goto packed;
		case SDL_YVYU_OVERLAY:
			/* YUV 4:2:2 Y0,V0,Y1,U0 (16bpp)  */
			y0pack=0;
			v0pack=1;
			y1pack=2;
			u0pack=3;
			goto packed;
	}
	return;

planar:
	sdl.LockYUVOverlay(overlay);
	{
		int y;
		Uint8 *Y,*U,*V;
		int odd_line;
		int uvlen=(r->w)>>1;
		int uvoffset=overlay->pitches[1]*((r->y+1)>>1)+((r->x+1)>>1);

		odd_line=(r->y)&1;
		Y=overlay->pixels[0]+overlay->pitches[0]*(r->y)+(r->x);
		U=overlay->pixels[uplane]+uvoffset;
		V=overlay->pixels[vplane]+uvoffset;

		for(y=0; y<r->h; y++)
		{
			memset(Y, yuvc[0], r->w);
			/* Increment every line */
			Y+=overlay->pitches[0];
			if(odd_line) {
				/* Increment on odd lines */
				U+=overlay->pitches[uplane];
				V+=overlay->pitches[vplane];
			}
			else {
				memset(U, yuvc[1], uvlen);
				memset(V, yuvc[2], uvlen);
			}
			odd_line = !odd_line;
		}
	}
	sdl.UnlockYUVOverlay(overlay);
	return;
packed:
	sdl.LockYUVOverlay(overlay);
	{
		int x,y;
		Uint32 colour;
		Uint8 *colour_array=(Uint8 *)&colour;
		Uint32 *offset;

		colour_array[y0pack]=yuvc[0];
		colour_array[y1pack]=yuvc[0];
		colour_array[u0pack]=yuvc[1];
		colour_array[v0pack]=yuvc[2];
		offset=(Uint32 *)(overlay->pixels[0]+overlay->pitches[0]*(r->y));
		offset+=(r->x>>1);
		for(y=0; y<r->h; y++)
		{
			for(x=0; x<r->w; x+=2)
				offset[x>>1]=colour;
			offset+=overlay->pitches[0]>>2;
		}
	}
	sdl.UnlockYUVOverlay(overlay);
	return;
}

static void sdl_user_func(int func, ...)
{
	va_list argptr;
	SDL_Event	ev;
	int rv;

	ev.type=SDL_USEREVENT;
	ev.user.data1=NULL;
	ev.user.data2=NULL;
	ev.user.code=func;
	sdl.mutexP(sdl_ufunc_mtx);
	/* Drain the swamp */
	if(sdl_x11available && sdl_using_x11)
		while(sdl.SemWaitTimeout(sdl_ufunc_rec, 0)==0);
	while (1) {
		va_start(argptr, func);
		switch(func) {
			case SDL_USEREVENT_SETICON:
				ev.user.data1=va_arg(argptr, void *);
				if((ev.user.data2=(unsigned long *)malloc(sizeof(unsigned long)))==NULL) {
					sdl.mutexV(sdl_ufunc_mtx);
					va_end(argptr);
					return;
				}
				*(unsigned long *)ev.user.data2=va_arg(argptr, unsigned long);
				break;
			case SDL_USEREVENT_SETNAME:
			case SDL_USEREVENT_SETTITLE:
				if((ev.user.data1=strdup(va_arg(argptr, char *)))==NULL) {
					sdl.mutexV(sdl_ufunc_mtx);
					va_end(argptr);
					return;
				}
				break;
			case SDL_USEREVENT_COPY:
			case SDL_USEREVENT_PASTE:
			case SDL_USEREVENT_SHOWMOUSE:
			case SDL_USEREVENT_HIDEMOUSE:
			case SDL_USEREVENT_FLUSH:
				break;
			default:
				va_end(argptr);
				return;
		}
		va_end(argptr);
		while((rv = sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff))!=1)
			YIELD();
		if (func != SDL_USEREVENT_FLUSH) {
			if(sdl_x11available && sdl_using_x11)
				if ((rv = sdl.SemWaitTimeout(sdl_ufunc_rec, 2000)) != 0)
					continue;
		}
		break;
	}
	sdl.mutexV(sdl_ufunc_mtx);
}

/* Called from main thread only */
static int sdl_user_func_ret(int func, ...)
{
	int rv;
	va_list argptr;
	SDL_Event	ev;

	ev.type=SDL_USEREVENT;
	ev.user.data1=NULL;
	ev.user.data2=NULL;
	ev.user.code=func;
	va_start(argptr, func);
	sdl.mutexP(sdl_ufunc_mtx);
	/* Drain the swamp */
	if(sdl_x11available && sdl_using_x11)
		while(sdl.SemWaitTimeout(sdl_ufunc_rec, 0)==0);
	while(1) {
		switch(func) {
			case SDL_USEREVENT_SETVIDMODE:
			case SDL_USEREVENT_INIT:
			case SDL_USEREVENT_QUIT:
				while(sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1)
					YIELD();
				break;
			default:
				sdl.mutexV(sdl_ufunc_mtx);
				va_end(argptr);
				return -1;
		}
		/*
		 * This is needed for lost event detection.
		 * Lost events only occur on SYSWMEVENT which is what
		 * we need for copy/paste on X11.
		 * This hack can be removed for SDL2
		 */
		if(sdl_x11available && sdl_using_x11)
			if((rv = sdl.SemWaitTimeout(sdl_ufunc_rec, 2000))!=0)
				continue;
		rv = sdl.SemWait(sdl_ufunc_ret);
		if(rv==0)
			break;
	}
	sdl.mutexV(sdl_ufunc_mtx);
	va_end(argptr);
	return(sdl_ufunc_retval);
}

static void exit_sdl_con(void)
{
	sdl_user_func_ret(SDL_USEREVENT_QUIT);
}

void sdl_copytext(const char *text, size_t buflen)
{
#if (defined(__MACH__) && defined(__APPLE__))
	if(!sdl_using_x11) {
#if defined(USE_PASTEBOARD)
		if (text && buflen)
			OSX_copytext(text, buflen);
		return;
#endif
#if defined(USE_SCRAP_MANAGER)
		ScrapRef	scrap;
		if(text && buflen) {
			if(!ClearCurrentScrap()) {		/* purge the current contents of the scrap. */
				if(!GetCurrentScrap(&scrap)) {		/* obtain a reference to the current scrap. */
					PutScrapFlavor(scrap, kScrapFlavorTypeText, /* kScrapFlavorMaskTranslated */ kScrapFlavorMaskNone, buflen, text); 		/* write the data to the scrap */
				}
			}
		}
		return;
#endif
	}
#endif

#if !defined(NO_X) && defined(__unix__)
	if(sdl_x11available && sdl_using_x11) {
		sdl.mutexP(sdl_copybuf_mutex);
		FREE_AND_NULL(sdl_copybuf);

		sdl_copybuf=(char *)malloc(buflen+1);
		if(sdl_copybuf!=NULL) {
			strcpy(sdl_copybuf, text);
			sdl_user_func(SDL_USEREVENT_COPY,0,0,0,0);
		}
		sdl.mutexV(sdl_copybuf_mutex);
		return;
	}
#endif

	sdl.mutexP(sdl_copybuf_mutex);
	FREE_AND_NULL(sdl_copybuf);

	sdl_copybuf=strdup(text);
	sdl.mutexV(sdl_copybuf_mutex);
	return;
}

char *sdl_getcliptext(void)
{
	char *ret=NULL;

#if (defined(__MACH__) && defined(__APPLE__))
	if(!sdl_using_x11) {
#if defined(USE_PASTEBOARD)
		return OSX_getcliptext();
#endif
#if defined(USE_SCRAP_MANAGER)
		ScrapRef	scrap;
		UInt32	fl;
		Size		scraplen;

		if(!GetCurrentScrap(&scrap)) {		/* obtain a reference to the current scrap. */
			if(!GetScrapFlavorFlags(scrap, kScrapFlavorTypeText, &fl) /* && (fl & kScrapFlavorMaskTranslated) */) {
				if(!GetScrapFlavorSize(scrap, kScrapFlavorTypeText, &scraplen)) {
					ret=(char *)malloc(scraplen+1);
					if(ret!=NULL) {
						if(GetScrapFlavorData(scrap, kScrapFlavorTypeText, &scraplen, sdl_pastebuf))
							ret[scraplen]=0;
					}
				}
			}
		}
		return ret;
#endif
	}
#endif

#if !defined(NO_X) && defined(__unix__)
	if(sdl_x11available && sdl_using_x11) {
		sdl_user_func(SDL_USEREVENT_PASTE,0,0,0,0);
		sdl.SemWait(sdl_pastebuf_set);
		if(sdl_pastebuf!=NULL) {
			ret=(char *)malloc(strlen(sdl_pastebuf)+1);
			if(ret!=NULL)
				strcpy(ret,sdl_pastebuf);
		}
		else
			ret=NULL;
		sdl.SemPost(sdl_pastebuf_copied);
		return(ret);
	}
#endif
	sdl.mutexP(sdl_copybuf_mutex);
	if(sdl_copybuf)
		ret=strdup(sdl_copybuf);
	sdl.mutexV(sdl_copybuf_mutex);
	return(ret);
}

void sdl_drawrect(struct rectlist *data)
{
	if(sdl_init_good) {
		data->next = NULL;
		sdl.mutexP(sdl_headlock);
		if (update_list == NULL)
			update_list = update_list_tail = data;
		else {
			update_list_tail->next = data;
			update_list_tail = data;
		}
		sdl.mutexV(sdl_headlock);
	}
	else
		bitmap_drv_free_rect(data);
}

void sdl_flush(void)
{
	sdl_user_func(SDL_USEREVENT_FLUSH);
}

static int sdl_init_mode(int mode)
{
    int oldcols;

	oldcols = cvstat.cols;

	sdl_user_func(SDL_USEREVENT_FLUSH);

	pthread_mutex_lock(&vstatlock);
	bitmap_drv_init_mode(mode, &bitmap_width, &bitmap_height);
	if(yuv.enabled)
		vstat.scaling = 2;
	cvstat = vstat;
	pthread_mutex_unlock(&vstatlock);

	/* Deal with 40 col doubling */
	if(!yuv.enabled) {
		if(oldcols != cvstat.cols) {
			if(oldcols == 40)
				cvstat.scaling /= 2;
			if(cvstat.cols == 40)
				cvstat.scaling *= 2;
		}
	}

	if(cvstat.scaling < 1)
		cvstat.scaling = 1;
	if(cvstat.vmultiplier < 1)
		cvstat.vmultiplier = 1;

	sdl_user_func_ret(SDL_USEREVENT_SETVIDMODE);

    return(0);
}

/* Called from main thread only (Passes Event) */
int sdl_init(int mode)
{
#if !defined(NO_X) && defined(__unix__)
	dll_handle	dl;
	const char *libnames[2]={"X11", NULL};
#endif

	if(init_sdl_video()) {
		fprintf(stderr, "SDL Video Init Failed\n");
		return(-1);
	}

	bitmap_drv_init(sdl_drawrect, sdl_flush);
	sdl.mutexP(bitmap_init_mutex);
	bitmap_initialized=1;
	sdl.mutexV(bitmap_init_mutex);

	if(mode==CIOLIB_MODE_SDL_FULLSCREEN)
		fullscreen=1;
	if(mode==CIOLIB_MODE_SDL_YUV)
		yuv.enabled=1;
	if(mode==CIOLIB_MODE_SDL_YUV_FULLSCREEN) {
		yuv.enabled=1;
		fullscreen=1;
	}
#if (SDL_MAJOR_VERSION > 1) || (SDL_MINOR_VERSION > 2) || (SDL_PATCHLEVEL > 9)
	if(yuv.enabled) {
		const SDL_version	*linked=sdl.Linked_Version();
		if(linked->major > 1 || linked->minor > 2 || linked->patch > 9) {
			yuv.screen_width=sdl.initial_videoinfo.current_w;
			yuv.screen_height=sdl.initial_videoinfo.current_h;
		}
	}
#endif
	sdl_init_mode(3);
	if(yuv.enabled && yuv.overlay==NULL) {
		fprintf(stderr, "YUV Enabled, but overlay is NULL\n");
		sdl_init_good=0;
	}
	sdl_user_func_ret(SDL_USEREVENT_INIT);

	if(sdl_init_good) {
		cio_api.mode=fullscreen?CIOLIB_MODE_SDL_FULLSCREEN:CIOLIB_MODE_SDL;
#ifdef _WIN32
		FreeConsole();
#endif
#if !defined(NO_X) && defined(__unix__)
		dl=xp_dlopen(libnames,RTLD_LAZY|RTLD_GLOBAL,7);
		if(dl!=NULL) {
			sdl_x11available=TRUE;
			if(sdl_x11available && (sdl_x11.XFree=xp_dlsym(dl,XFree))==NULL) {
				xp_dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XGetSelectionOwner=xp_dlsym(dl,XGetSelectionOwner))==NULL) {
				xp_dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XConvertSelection=xp_dlsym(dl,XConvertSelection))==NULL) {
				xp_dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XGetWindowProperty=xp_dlsym(dl,XGetWindowProperty))==NULL) {
				xp_dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XChangeProperty=xp_dlsym(dl,XChangeProperty))==NULL) {
				xp_dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XSendEvent=xp_dlsym(dl,XSendEvent))==NULL) {
				xp_dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XSetSelectionOwner=xp_dlsym(dl,XSetSelectionOwner))==NULL) {
				xp_dlclose(dl);
				sdl_x11available=FALSE;
			}
		}
#else
		sdl_x11available=FALSE;
#endif
		cio_api.options |= CONIO_OPT_PALETTE_SETTING | CONIO_OPT_SET_TITLE | CONIO_OPT_SET_NAME | CONIO_OPT_SET_ICON;
		return(0);
	}

	return(-1);
}

void sdl_setscaling(int new_value)
{
	if (yuv.enabled)
		return;
	pthread_mutex_lock(&vstatlock);
	cvstat.scaling = vstat.scaling = new_value;
	pthread_mutex_unlock(&vstatlock);
}

int sdl_getscaling(void)
{
	if (yuv.enabled)
		return 1;
	return cvstat.scaling;
}

/* Called from main thread only */
int sdl_kbhit(void)
{
	int ret;

	sdl.mutexP(sdl_keylock);
	ret=(sdl_key!=sdl_keynext);
	sdl.mutexV(sdl_keylock);
	return(ret);
}

/* Called from main thread only */
int sdl_getch(void)
{
	int ch;

	sdl.SemWait(sdl_key_pending);
	sdl.mutexP(sdl_keylock);

	/* This always frees up space in keybuf for one more char */
	ch=sdl_keybuf[sdl_key++];
	/* If we have missed mouse keys, tack them on to the end of the buffer now */
	if(sdl_pending_mousekeys) {
		if(sdl_pending_mousekeys & 1)	/* Odd number... second char */
	       	sdl_keybuf[sdl_keynext++]=CIO_KEY_MOUSE >> 8;
		else							/* Even number... first char */
	        sdl_keybuf[sdl_keynext++]=CIO_KEY_MOUSE & 0xff;
        sdl.SemPost(sdl_key_pending);
		sdl_pending_mousekeys--;
	}
	sdl.mutexV(sdl_keylock);
	return(ch);
}

/* Called from main thread only */
void sdl_textmode(int mode)
{
	sdl_init_mode(mode);
}

/* Called from main thread only (Passes Event) */
int sdl_setname(const char *name)
{
	sdl_user_func(SDL_USEREVENT_SETNAME,name);
	return(0);
}

/* Called from main thread only (Passes Event) */
int sdl_seticon(const void *icon, unsigned long size)
{
	sdl_user_func(SDL_USEREVENT_SETICON,icon,size);
	return(0);
}

/* Called from main thread only (Passes Event) */
int sdl_settitle(const char *title)
{
	sdl_user_func(SDL_USEREVENT_SETTITLE,title);
	return(0);
}

int sdl_showmouse(void)
{
	sdl_user_func(SDL_USEREVENT_SHOWMOUSE);
	return(1);
}

int sdl_hidemouse(void)
{
	sdl_user_func(SDL_USEREVENT_HIDEMOUSE);
	return(0);
}

int sdl_get_window_info(int *width, int *height, int *xpos, int *ypos)
{
	sdl.mutexP(win_mutex);
	if(width)
		*width=win->h;
	if(height)
		*height=win->h;
	if(xpos)
		*xpos=-1;
	if(ypos)
		*ypos=-1;
	sdl.mutexV(win_mutex);

	return(1);
}

static void setup_surfaces(void)
{
	int		char_width;
	int		char_height;
	int		flags=SDL_HWSURFACE|SDL_ANYFORMAT;
	SDL_Surface	*tmp_rect;
	SDL_Event	ev;
	int charwidth, charheight, cols, scaling, rows, vmultiplier;

	if(fullscreen)
		flags |= SDL_FULLSCREEN;
	else
		flags |= SDL_RESIZABLE;

	sdl.mutexP(win_mutex);
	charwidth = cvstat.charwidth;
	charheight = cvstat.charheight;
	cols = cvstat.cols;
	scaling = cvstat.scaling;
	rows = cvstat.rows;
	vmultiplier = cvstat.vmultiplier;
	
	char_width=charwidth*cols*scaling;
	char_height=charheight*rows*scaling*vmultiplier;

	if(yuv.enabled) {
		if(!yuv.win_width)
			yuv.win_width=charwidth*cols;
		if(!yuv.win_height)
			yuv.win_height=charheight*rows;
		if(fullscreen && yuv.screen_width && yuv.screen_height)
			win=sdl.SetVideoMode(yuv.screen_width,yuv.screen_height,0,flags);
		else
			win=sdl.SetVideoMode(yuv.win_width,yuv.win_height,0,flags);
	}
	else
		win=sdl.SetVideoMode(char_width,char_height,0,flags);

#if !defined(NO_X) && defined(__unix__)
	if(sdl_x11available && sdl_using_x11) {
		XEvent respond;
		SDL_SysWMinfo	wmi;
		SDL_VERSION(&(wmi.version));
		sdl.GetWMInfo(&wmi);
		respond.type=ConfigureNotify;
		respond.xconfigure.height = win->h;
		respond.xconfigure.width = win->w;
		sdl_x11.XSendEvent(wmi.info.x11.display,wmi.info.x11.window,0,0,&respond);
	}
#endif

	if(win!=NULL) {
		sdl.mutexP(newrect_mutex);
		if(new_rect)
			sdl.FreeSurface(new_rect);
		new_rect=NULL;
		tmp_rect=sdl.CreateRGBSurface(SDL_HWSURFACE
				, char_width
				, char_height
				, 32, 0, 0, 0, 0);
		if(tmp_rect) {
			if(yuv.enabled) {
				new_rect=tmp_rect;
			}
			else {
				new_rect=sdl.DisplayFormat(tmp_rect);
				sdl.FreeSurface(tmp_rect);
			}
		}
		sdl.mutexV(newrect_mutex);
		if(yuv.enabled) {
			if(yuv.overlay)
				sdl.FreeYUVOverlay(yuv.overlay);
			if(yuv.best_format==0) {
				yuv.overlay=sdl.CreateYUVOverlay(char_width,char_height, SDL_YV12_OVERLAY, win);
				if(yuv.overlay)
					yuv.best_format=yuv.overlay->format;
				if(yuv.overlay==NULL || !yuv.overlay->hw_overlay) {
					if (yuv.overlay)
						sdl.FreeYUVOverlay(yuv.overlay);
					yuv.overlay=sdl.CreateYUVOverlay(char_width,char_height, SDL_YUY2_OVERLAY, win);
					if(yuv.overlay)
						yuv.best_format=yuv.overlay->format;
					if(yuv.overlay==NULL || !yuv.overlay->hw_overlay) {
						if (yuv.overlay)
							sdl.FreeYUVOverlay(yuv.overlay);
						yuv.overlay=sdl.CreateYUVOverlay(char_width,char_height, SDL_YVYU_OVERLAY, win);
						if(yuv.overlay)
							yuv.best_format=yuv.overlay->format;
						if(yuv.overlay==NULL || !yuv.overlay->hw_overlay) {
							if (yuv.overlay)
								sdl.FreeYUVOverlay(yuv.overlay);
							yuv.overlay=sdl.CreateYUVOverlay(char_width,char_height, SDL_UYVY_OVERLAY, win);
							if(yuv.overlay)
								yuv.best_format=yuv.overlay->format;
							if(yuv.overlay==NULL || !yuv.overlay->hw_overlay) {
								if (yuv.overlay)
									sdl.FreeYUVOverlay(yuv.overlay);
								yuv.overlay=sdl.CreateYUVOverlay(char_width,char_height, SDL_IYUV_OVERLAY, win);
								if(yuv.overlay)
									yuv.best_format=yuv.overlay->format;
							}
						}
					}
				}
				if(yuv.overlay)
					sdl.FreeYUVOverlay(yuv.overlay);
			}
			yuv.overlay=sdl.CreateYUVOverlay(char_width,char_height, yuv.best_format, win);
		}
		sdl.mutexP(newrect_mutex);
		sdl.mutexV(newrect_mutex);
		bitmap_drv_request_pixels();
	}
	else if(sdl_init_good) {
		ev.type=SDL_QUIT;
		sdl_exitcode=1;
		sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff);
	}
	sdl.mutexV(win_mutex);
}

/* Called from event thread only */
static void sdl_add_key(unsigned int keyval)
{
	if(keyval==0xa600) {
		fullscreen=!fullscreen;
		if(yuv.enabled)
			cio_api.mode=fullscreen?CIOLIB_MODE_SDL_YUV_FULLSCREEN:CIOLIB_MODE_SDL_YUV;
		else
			cio_api.mode=fullscreen?CIOLIB_MODE_SDL_FULLSCREEN:CIOLIB_MODE_SDL;
		setup_surfaces();
		return;
	}
	if(keyval <= 0xffff) {
		sdl.mutexP(sdl_keylock);
		if(sdl_keynext+1==sdl_key) {
			beep();
			sdl.mutexV(sdl_keylock);
			return;
		}
		if((sdl_keynext+2==sdl_key) && keyval > 0xff) {
			if(keyval==CIO_KEY_MOUSE)
				sdl_pending_mousekeys+=2;
			else
				beep();
			sdl.mutexV(sdl_keylock);
			return;
		}
		sdl_keybuf[sdl_keynext++]=keyval & 0xff;
		sdl.SemPost(sdl_key_pending);
		if(keyval>0xff) {
			sdl_keybuf[sdl_keynext++]=keyval >> 8;
			sdl.SemPost(sdl_key_pending);
		}
		sdl.mutexV(sdl_keylock);
	}
}

static unsigned int cp437_convert(unsigned int unicode)
{
	if(unicode < 0x80)
		return(unicode);
	switch(unicode) {
		case 0x00c7:
			return(0x80);
		case 0x00fc:
			return(0x81);
		case 0x00e9:
			return(0x82);
		case 0x00e2:
			return(0x83);
		case 0x00e4:
			return(0x84);
		case 0x00e0:
			return(0x85);
		case 0x00e5:
			return(0x86);
		case 0x00e7:
			return(0x87);
		case 0x00ea:
			return(0x88);
		case 0x00eb:
			return(0x89);
		case 0x00e8:
			return(0x8a);
		case 0x00ef:
			return(0x8b);
		case 0x00ee:
			return(0x8c);
		case 0x00ec:
			return(0x8d);
		case 0x00c4:
			return(0x8e);
		case 0x00c5:
			return(0x8f);
		case 0x00c9:
			return(0x90);
		case 0x00e6:
			return(0x91);
		case 0x00c6:
			return(0x92);
		case 0x00f4:
			return(0x93);
		case 0x00f6:
			return(0x94);
		case 0x00f2:
			return(0x95);
		case 0x00fb:
			return(0x96);
		case 0x00f9:
			return(0x97);
		case 0x00ff:
			return(0x98);
		case 0x00d6:
			return(0x99);
		case 0x00dc:
			return(0x9a);
		case 0x00a2:
			return(0x9b);
		case 0x00a3:
			return(0x9c);
		case 0x00a5:
			return(0x9d);
		case 0x20a7:
			return(0x9e);
		case 0x0192:
			return(0x9f);
		case 0x00e1:
			return(0xa0);
		case 0x00ed:
			return(0xa1);
		case 0x00f3:
			return(0xa2);
		case 0x00fa:
			return(0xa3);
		case 0x00f1:
			return(0xa4);
		case 0x00d1:
			return(0xa5);
		case 0x00aa:
			return(0xa6);
		case 0x00ba:
			return(0xa7);
		case 0x00bf:
			return(0xa8);
		case 0x2310:
			return(0xa9);
		case 0x00ac:
			return(0xaa);
		case 0x00bd:
			return(0xab);
		case 0x00bc:
			return(0xac);
		case 0x00a1:
			return(0xad);
		case 0x00ab:
			return(0xae);
		case 0x00bb:
			return(0xaf);
		case 0x2591:
			return(0xb0);
		case 0x2592:
			return(0xb1);
		case 0x2593:
			return(0xb2);
		case 0x2502:
			return(0xb3);
		case 0x2524:
			return(0xb4);
		case 0x2561:
			return(0xb5);
		case 0x2562:
			return(0xb6);
		case 0x2556:
			return(0xb7);
		case 0x2555:
			return(0xb8);
		case 0x2563:
			return(0xb9);
		case 0x2551:
			return(0xba);
		case 0x2557:
			return(0xbb);
		case 0x255d:
			return(0xbc);
		case 0x255c:
			return(0xbd);
		case 0x255b:
			return(0xbe);
		case 0x2510:
			return(0xbf);
		case 0x2514:
			return(0xc0);
		case 0x2534:
			return(0xc1);
		case 0x252c:
			return(0xc2);
		case 0x251c:
			return(0xc3);
		case 0x2500:
			return(0xc4);
		case 0x253c:
			return(0xc5);
		case 0x255e:
			return(0xc6);
		case 0x255f:
			return(0xc7);
		case 0x255a:
			return(0xc8);
		case 0x2554:
			return(0xc9);
		case 0x2569:
			return(0xca);
		case 0x2566:
			return(0xcb);
		case 0x2560:
			return(0xcc);
		case 0x2550:
			return(0xcd);
		case 0x256c:
			return(0xce);
		case 0x2567:
			return(0xcf);
		case 0x2568:
			return(0xd0);
		case 0x2564:
			return(0xd1);
		case 0x2565:
			return(0xd2);
		case 0x2559:
			return(0xd3);
		case 0x2558:
			return(0xd4);
		case 0x2552:
			return(0xd5);
		case 0x2553:
			return(0xd6);
		case 0x256b:
			return(0xd7);
		case 0x256a:
			return(0xd8);
		case 0x2518:
			return(0xd9);
		case 0x250c:
			return(0xda);
		case 0x2588:
			return(0xdb);
		case 0x2584:
			return(0xdc);
		case 0x258c:
			return(0xdd);
		case 0x2590:
			return(0xde);
		case 0x2580:
			return(0xdf);
		case 0x03b1:
			return(0xe0);
		case 0x00df:
			return(0xe1);
		case 0x0393:
			return(0xe2);
		case 0x03c0:
			return(0xe3);
		case 0x03a3:
			return(0xe4);
		case 0x03c3:
			return(0xe5);
		case 0x00b5:
			return(0xe6);
		case 0x03c4:
			return(0xe7);
		case 0x03a6:
			return(0xe8);
		case 0x0398:
			return(0xe9);
		case 0x03a9:
			return(0xea);
		case 0x03b4:
			return(0xeb);
		case 0x221e:
			return(0xec);
		case 0x03c6:
			return(0xed);
		case 0x03b5:
			return(0xee);
		case 0x2229:
			return(0xef);
		case 0x2261:
			return(0xf0);
		case 0x00b1:
			return(0xf1);
		case 0x2265:
			return(0xf2);
		case 0x2264:
			return(0xf3);
		case 0x2320:
			return(0xf4);
		case 0x2321:
			return(0xf5);
		case 0x00f7:
			return(0xf6);
		case 0x2248:
			return(0xf7);
		case 0x00b0:
			return(0xf8);
		case 0x2219:
			return(0xf9);
		case 0x00b7:
			return(0xfa);
		case 0x221a:
			return(0xfb);
		case 0x207f:
			return(0xfc);
		case 0x00b2:
			return(0xfd);
		case 0x25a0:
			return(0xfe);
		case 0x00a0:
			return(0xff);
	}
	return(0x01ffff);
}

/* Called from event thread only */
static unsigned int sdl_get_char_code(unsigned int keysym, unsigned int mod, unsigned int unicode)
{
	int expect;
	int i;

#ifdef __DARWIN__
	if(unicode==0x7f) {
		unicode=0x08;
		keysym=SDLK_BACKSPACE;
	}
#endif

	/*
	 * No Unicode translation available.
	 * Or there *IS* an SDL keysym.
	 * Or ALT (Meta) pressed
	 */
	if((!unicode) || (keysym > SDLK_FIRST && keysym < SDLK_LAST) || (mod & (KMOD_META|KMOD_ALT))) {

		/* Find the SDL keysym */
		for(i=0;sdl_keyval[i].keysym;i++) {
			if(sdl_keyval[i].keysym==keysym) {
				/* KeySym found in table */

				/*
				 * Using the modifiers, look up the expected scan code.
				 * Under windows, this is what unicode will be set to
				 * if the ALT key is not AltGr
				 */

				if(mod & KMOD_CTRL)
					expect=sdl_keyval[i].ctrl;
				else if(mod & KMOD_SHIFT) {
					if(mod & KMOD_CAPS)
						expect=sdl_keyval[i].key;
					else
						expect=sdl_keyval[i].shift;
				}
				else {
					if(mod & KMOD_CAPS && (toupper(sdl_keyval[i].key) == sdl_keyval[i].shift))
						expect=sdl_keyval[i].shift;
					else
						expect=sdl_keyval[i].key;
				}

				/*
				 * Now handle the ALT case so that expect will
				 * be what we expect to return
				 */
				if(mod & (KMOD_META|KMOD_ALT)) {

					/* Yes, this is a "normal" ALT combo */
					if(unicode==expect || unicode == 0)
						return(sdl_keyval[i].alt);

					/* AltGr apparently... translate unicode or give up */
					return(cp437_convert(unicode));
				}

				/*
				 * If the keysym is a keypad one
				 * AND numlock is locked
				 * AND none of Control, Shift, ALT, or Meta are pressed
				 */
				if(keysym >= SDLK_KP0 && keysym <= SDLK_KP_EQUALS && 
						(!(mod & (KMOD_CTRL|KMOD_SHIFT|KMOD_ALT|KMOD_META) ))) {
#if defined(_WIN32)
					/*
					 * Apparently, Win32 SDL doesn't interpret keypad with numlock...
					 * and doesn't know the state of numlock either...
					 * So, do that here. *sigh*
					 */

					mod &= ~KMOD_NUM;	// Ignore "current" mod state
					if (GetKeyState(VK_NUMLOCK) & 1)
						mod |= KMOD_NUM;
#endif
					if (mod & KMOD_NUM) {
						switch(keysym) {
							case SDLK_KP0:
								return('0');
							case SDLK_KP1:
								return('1');
							case SDLK_KP2:
								return('2');
							case SDLK_KP3:
								return('3');
							case SDLK_KP4:
								return('4');
							case SDLK_KP5:
								return('5');
							case SDLK_KP6:
								return('6');
							case SDLK_KP7:
								return('7');
							case SDLK_KP8:
								return('8');
							case SDLK_KP9:
								return('9');
							case SDLK_KP_PERIOD:
								return('.');
							case SDLK_KP_DIVIDE:
								return('/');
							case SDLK_KP_MULTIPLY:
								return('*');
							case SDLK_KP_MINUS:
								return('-');
							case SDLK_KP_PLUS:
								return('+');
							case SDLK_KP_ENTER:
								return('\r');
							case SDLK_KP_EQUALS:
								return('=');
						}
					}
				}

				/*
				 * "Extended" keys are always right since we can't compare to unicode
				 * This does *NOT* mean ALT-x, it means things like F1 and Print Screen
				 */
				if(sdl_keyval[i].key > 255)			/* Extended regular key */
					return(expect);

				/*
				 * If there is no unicode translation available,
				 * we *MUST* use our table since we have
				 * no other data to use.  This is apparently
				 * never true on OS X.
				 */
				if(!unicode)
					return(expect);

				/*
				 * At this point, we no longer have a reason to distrust the
				 * unicode mapping.  If we can coerce it into CP437, we will.
				 * If we can't, just give up.
				 */
				return(cp437_convert(unicode));
			}
		}
	}
	/*
	 * Well, we can't find it in our table...
	 * If there's a unicode character, use that if possible.
	 */
	if(unicode)
		return(cp437_convert(unicode));

	/*
	 * No unicode... perhaps it's ASCII?
	 * Most likely, this would be a strangely
	 * entered control character.
	 *
	 * If *any* modifier key is down though
	 * we're not going to trust the keysym
	 * value since we can't.
	 */
	if(keysym <= 127 && !(mod & (KMOD_META|KMOD_ALT|KMOD_CTRL|KMOD_SHIFT)))
		return(keysym);

	/* Give up.  It's not working out for us. */
	return(0x0001ffff);
}

/* Mouse event/keyboard thread */
static int sdl_mouse_thread(void *data)
{
	SetThreadName("SDL Mouse");
	while(1) {
		if(mouse_wait())
			sdl_add_key(CIO_KEY_MOUSE);
	}
	return 0;
}

static int win_to_text_xpos(int winpos)
{
	int ret;

	if(yuv.enabled) {

		sdl.mutexP(win_mutex);
		ret = winpos*cvstat.cols/win->w+1;
		sdl.mutexV(win_mutex);
		return(ret);
	}
	else {
		ret = winpos/(cvstat.charwidth*cvstat.scaling)+1;
		return ret;
	}
}

static int win_to_text_ypos(int winpos)
{
	int ret;

	if(yuv.enabled) {
		sdl.mutexP(win_mutex);
		ret = winpos*cvstat.rows/win->h+1;
		sdl.mutexV(win_mutex);
		return(ret);
	}
	else {
		ret = winpos/(cvstat.charheight*cvstat.scaling*cvstat.vmultiplier)+1;
		return ret;
	}
}

static int sdl_video_event_thread(void *data)
{
	SDL_Event	ev;
	int			new_scaling = -1;
	int			old_scaling;
	SDL_Rect	upd_rect;

	while(1) {
		sdl.mutexP(bitmap_init_mutex);
		if(bitmap_initialized) {
			sdl.mutexV(bitmap_init_mutex);
			break;
		}
		sdl.mutexV(bitmap_init_mutex);
		SLEEP(1);
	}
	old_scaling = cvstat.scaling;
	
	if(!init_sdl_video()) {
		char	driver[16];
		if(sdl.VideoDriverName(driver, sizeof(driver))!=NULL) {
#if defined(_WIN32)
			if(!strcmp(driver,"directx"))
				sdl_using_directx=TRUE;
#else
			sdl_using_directx=FALSE;
#endif
#if (defined(__MACH__) && defined(__APPLE__))
			if(!strcmp(driver,"Quartz"))
				sdl_using_quartz=TRUE;
#else
			sdl_using_quartz=FALSE;
#endif
#if !defined(NO_X) && defined(__unix__)
			if(!strcmp(driver,"x11"))
				sdl_using_x11=TRUE;
			if(!strcmp(driver,"dga"))
				sdl_using_x11=TRUE;
#else
			sdl_using_x11=FALSE;
#endif
		}

		while(1) {
			if(sdl.PollEvent(&ev)!=1) {
				if (new_scaling != -1 || cvstat.scaling != old_scaling) {
					if (new_scaling == -1)
						new_scaling = cvstat.scaling;
					sdl_setscaling(new_scaling);
					new_scaling = -1;
					if(cvstat.scaling < 1)
						sdl_setscaling(1);
					setup_surfaces();
					old_scaling = cvstat.scaling;
				}
				SLEEP(1);
			}
			else {
				switch (ev.type) {
					case SDL_ACTIVEEVENT:		/* Focus change */
						break;
					case SDL_KEYDOWN:			/* Keypress */
						sdl_add_key(sdl_get_char_code(ev.key.keysym.sym, ev.key.keysym.mod, ev.key.keysym.unicode));
						break;
					case SDL_KEYUP:				/* Ignored (handled in KEYDOWN event) */
						break;
					case SDL_MOUSEMOTION:
						if(!ciolib_mouse_initialized)
							break;
						ciomouse_gotevent(CIOLIB_MOUSE_MOVE,win_to_text_xpos(ev.motion.x),win_to_text_ypos(ev.motion.y));
						break;
					case SDL_MOUSEBUTTONDOWN:
						if(!ciolib_mouse_initialized)
							break;
						switch(ev.button.button) {
							case SDL_BUTTON_LEFT:
								ciomouse_gotevent(CIOLIB_BUTTON_PRESS(1),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
								break;
							case SDL_BUTTON_MIDDLE:
								ciomouse_gotevent(CIOLIB_BUTTON_PRESS(2),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
								break;
							case SDL_BUTTON_RIGHT:
								ciomouse_gotevent(CIOLIB_BUTTON_PRESS(3),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
								break;
						}
						break;
					case SDL_MOUSEBUTTONUP:
						if(!ciolib_mouse_initialized)
							break;
						switch(ev.button.button) {
							case SDL_BUTTON_LEFT:
								ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(1),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
								break;
							case SDL_BUTTON_MIDDLE:
								ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(2),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
								break;
							case SDL_BUTTON_RIGHT:
								ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(3),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
								break;
						}
						break;
					case SDL_QUIT:
						if (ciolib_reaper)
							exit(0);
						else
							sdl_add_key(CIO_KEY_QUIT);
						break;
					case SDL_VIDEORESIZE:
						if(ev.resize.w > 0 && ev.resize.h > 0) {
							if(yuv.enabled) {
								yuv.win_width=ev.resize.w;
								yuv.win_height=ev.resize.h;
								new_scaling = 2;
							}
							else {
								new_scaling = (int)(ev.resize.w/(cvstat.charwidth*cvstat.cols));
							}
						}
						break;
					case SDL_VIDEOEXPOSE:
						{
							if(yuv.enabled) {
								bitmap_drv_request_pixels();
							}
							else {
								upd_rect.x=0;
								upd_rect.y=0;
								sdl.mutexP(win_mutex);
								sdl.mutexP(newrect_mutex);
								upd_rect.w=new_rect->w;
								upd_rect.h=new_rect->h;
								sdl.BlitSurface(new_rect, &upd_rect, win, &upd_rect);
								sdl.mutexV(newrect_mutex);
								sdl.Flip(win);
								sdl.mutexV(win_mutex);
							}
						}
						break;
					case SDL_USEREVENT: {
						struct rectlist *list;
						struct rectlist *list_tail;
						struct rectlist *old_next;
						/* Tell SDL to do various stuff... */
						if (ev.user.code != SDL_USEREVENT_FLUSH)
							if(sdl_x11available && sdl_using_x11)
								sdl.SemPost(sdl_ufunc_rec);
						switch(ev.user.code) {
							case SDL_USEREVENT_QUIT:
								sdl_ufunc_retval=0;
								sdl.SemPost(sdl_ufunc_ret);
								return(0);
							case SDL_USEREVENT_FLUSH:
								sdl.mutexP(sdl_headlock);
								list = update_list;
								update_list = update_list_tail = NULL;
								sdl.mutexV(sdl_headlock);
								/* Old SDL_USEREVENT_UPDATERECT */
								for (; list; list = old_next) {
									SDL_Rect r;
									int x,y,offset;
									int scaling, vmultiplier;

									old_next = list->next;
									sdl.mutexP(win_mutex);
									if(!win) {
										sdl.mutexV(win_mutex);
										/* Put it back at the start of the list... */
										sdl.mutexP(sdl_headlock);
										for (list_tail = list; list_tail->next; list_tail = list_tail->next);
										list_tail->next = update_list;
										update_list = list;
										sdl.mutexV(sdl_headlock);
										break;
									}
									sdl.mutexP(newrect_mutex);
									scaling = cvstat.scaling;
									vmultiplier = cvstat.vmultiplier;
									r.w=scaling;
									r.h=scaling*vmultiplier;
									for(y=0; y<list->rect.height; y++) {
										offset=y*list->rect.width;
										r.y=(list->rect.y+y)*scaling*vmultiplier;
										for(x=0; x<list->rect.width; x++) {
											r.x=(list->rect.x+x)*scaling;
											if(yuv.enabled) {
												Uint8 yuvc[3];

												RGBtoYUV(list->data[offset] >> 16 & 0xff, list->data[offset] >> 8 & 0xff, list->data[offset] & 0xff, yuvc);
												yuv_fillrect(yuv.overlay, &r, yuvc);
											}
											else {
												sdl.FillRect(new_rect, &r, sdl.MapRGB(win->format, list->data[offset] >> 16 & 0xff, list->data[offset] >> 8 & 0xff, list->data[offset] & 0xff));
											}
											offset++;
										}
									}
									if(!yuv.enabled) {
										upd_rect.x=list->rect.x*scaling;
										upd_rect.y=list->rect.y*scaling*vmultiplier;
										upd_rect.w=list->rect.width*scaling;
										upd_rect.h=list->rect.height*scaling*vmultiplier;
										sdl.BlitSurface(new_rect, &upd_rect, win, &upd_rect);
									}
									sdl.mutexV(newrect_mutex);
									sdl.mutexV(win_mutex);
									bitmap_drv_free_rect(list);
								}

								/* Old flush function */
								sdl.mutexP(win_mutex);
								sdl.mutexP(newrect_mutex);
								if(win && new_rect) {
									if(yuv.enabled) {
										if(yuv.overlay && yuv.changed) {
											SDL_Rect	dstrect;
	
											yuv.changed=0;
											dstrect.w=win->w;
											dstrect.h=win->h;
											dstrect.x=0;
											dstrect.y=0;
											sdl.DisplayYUVOverlay(yuv.overlay, &dstrect);
										}
									}
									else {
										sdl.Flip(win);
									}
								}
								sdl.mutexP(newrect_mutex);
								sdl.mutexV(win_mutex);
								break;
							case SDL_USEREVENT_SETNAME:
								sdl.WM_SetCaption((char *)ev.user.data1,(char *)ev.user.data1);
								free(ev.user.data1);
								break;
							case SDL_USEREVENT_SETICON:
								if(sdl_icon != NULL)
									sdl.FreeSurface(sdl_icon);
								sdl_icon=sdl.CreateRGBSurfaceFrom(ev.user.data1
										, *(unsigned long *)ev.user.data2
										, *(unsigned long *)ev.user.data2
										, 32
										, *(unsigned long *)ev.user.data2*4
										, *(DWORD *)"\377\0\0\0"
										, *(DWORD *)"\0\377\0\0"
										, *(DWORD *)"\0\0\377\0"
										, *(DWORD *)"\0\0\0\377"
								);
								sdl.WM_SetIcon(sdl_icon,NULL);
								free(ev.user.data2);
								break;
							case SDL_USEREVENT_SETTITLE:
								sdl.WM_SetCaption((char *)ev.user.data1,NULL);
								free(ev.user.data1);
								break;
							case SDL_USEREVENT_SETVIDMODE:
								new_scaling = -1;
								old_scaling = cvstat.scaling;
								setup_surfaces();
								sdl_ufunc_retval=0;
								sdl.SemPost(sdl_ufunc_ret);
								break;
							case SDL_USEREVENT_HIDEMOUSE:
								sdl.ShowCursor(SDL_DISABLE);
								break;
							case SDL_USEREVENT_SHOWMOUSE:
								sdl.ShowCursor(SDL_ENABLE);
								break;
							case SDL_USEREVENT_INIT:
								if(!sdl_init_good) {
									if(sdl.WasInit(SDL_INIT_VIDEO)==SDL_INIT_VIDEO) {
										sdl.mutexP(win_mutex);
										if(win != NULL) {
											sdl.EnableUNICODE(1);
											sdl.EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
											mouse_thread=sdl.CreateThread(sdl_mouse_thread, NULL);
											sdl_init_good=1;
										}
										sdl.mutexV(win_mutex);
									}
								}
								sdl_ufunc_retval=0;
								sdl.SemPost(sdl_ufunc_ret);
								break;
							case SDL_USEREVENT_COPY:
#if !defined(NO_X) && defined(__unix__) && defined(SDL_VIDEO_DRIVER_X11)
								if(sdl_x11available && sdl_using_x11) {
									SDL_SysWMinfo	wmi;

									SDL_VERSION(&(wmi.version));
									sdl.GetWMInfo(&wmi);
									sdl.EventState(SDL_SYSWMEVENT, SDL_ENABLE);
									copy_needs_events = 1;
									sdl_x11.XSetSelectionOwner(wmi.info.x11.display, CONSOLE_CLIPBOARD, wmi.info.x11.window, CurrentTime);
								}
#endif
								break;
							case SDL_USEREVENT_PASTE:
#if !defined(NO_X) && defined(__unix__) && defined(SDL_VIDEO_DRIVER_X11)
								if(sdl_x11available && sdl_using_x11) {
									Window sowner=None;
									SDL_SysWMinfo	wmi;

									SDL_VERSION(&(wmi.version));
									sdl.GetWMInfo(&wmi);

									paste_needs_events = 1;
									sdl.EventState(SDL_SYSWMEVENT, SDL_ENABLE);
									sowner=sdl_x11.XGetSelectionOwner(wmi.info.x11.display, CONSOLE_CLIPBOARD);
									if(sowner==wmi.info.x11.window) {
										/* Get your own primary selection */
										if(sdl_copybuf==NULL) {
											FREE_AND_NULL(sdl_pastebuf);
										}
										else
											sdl_pastebuf=(char *)malloc(strlen(sdl_copybuf)+1);
										if(sdl_pastebuf!=NULL)
											strcpy(sdl_pastebuf,sdl_copybuf);
										/* Set paste buffer */
										sdl.SemPost(sdl_pastebuf_set);
										sdl.SemWait(sdl_pastebuf_copied);
										FREE_AND_NULL(sdl_pastebuf);
										paste_needs_events = 0;
										if (!copy_needs_events)
											sdl.EventState(SDL_SYSWMEVENT, SDL_DISABLE);
									}
									else if(sowner!=None) {
										sdl_x11.XConvertSelection(wmi.info.x11.display, CONSOLE_CLIPBOARD, XA_STRING, XA_STRING, wmi.info.x11.window, CurrentTime);
									}
									else {
										/* Set paste buffer */
										FREE_AND_NULL(sdl_pastebuf);
										sdl.SemPost(sdl_pastebuf_set);
										sdl.SemWait(sdl_pastebuf_copied);
										paste_needs_events = 0;
										if (!copy_needs_events)
											sdl.EventState(SDL_SYSWMEVENT, SDL_DISABLE);
									}
								}
#endif
								break;
						}
						break;
					}
					case SDL_SYSWMEVENT:			/* ToDo... This is where Copy/Paste needs doing */
#if !defined(NO_X) && defined(__unix__) && defined(SDL_VIDEO_DRIVER_X11)
						if(sdl_x11available && sdl_using_x11) {
							XEvent *e;
							e=&ev.syswm.msg->event.xevent;
							switch(e->type) {
								case SelectionClear: {
										XSelectionClearEvent *req;

										req=&(e->xselectionclear);
										sdl.mutexP(sdl_copybuf_mutex);
										if(req->selection==CONSOLE_CLIPBOARD) {
											FREE_AND_NULL(sdl_copybuf);
										}
										sdl.mutexV(sdl_copybuf_mutex);
										copy_needs_events = 0;
										if (!paste_needs_events)
											sdl.EventState(SDL_SYSWMEVENT, SDL_DISABLE);
										break;
								}
								case SelectionNotify: {
										int format=0;
										unsigned long len, bytes_left, dummy;
										Atom type;
										XSelectionEvent *req;
										SDL_SysWMinfo	wmi;

										SDL_VERSION(&(wmi.version));
										sdl.GetWMInfo(&wmi);
										req=&(e->xselection);
										if(req->requestor!=wmi.info.x11.window)
											break;
										if(req->property) {
											sdl_x11.XGetWindowProperty(wmi.info.x11.display, wmi.info.x11.window, req->property, 0, 0, 0, AnyPropertyType, &type, &format, &len, &bytes_left, (unsigned char **)(&sdl_pastebuf));
											if(bytes_left > 0 && format==8)
												sdl_x11.XGetWindowProperty(wmi.info.x11.display, wmi.info.x11.window, req->property,0,bytes_left,0,AnyPropertyType,&type,&format,&len,&dummy,(unsigned char **)&sdl_pastebuf);
											else {
												FREE_AND_NULL(sdl_pastebuf);
											}
										}
										else {
											FREE_AND_NULL(sdl_pastebuf);
										}

										/* Set paste buffer */
										sdl.SemPost(sdl_pastebuf_set);
										sdl.SemWait(sdl_pastebuf_copied);
										paste_needs_events = 0;
										if (!copy_needs_events)
											sdl.EventState(SDL_SYSWMEVENT, SDL_DISABLE);
										if(sdl_pastebuf!=NULL) {
											sdl_x11.XFree(sdl_pastebuf);
											sdl_pastebuf=NULL;
										}
										break;
								}
								case SelectionRequest: {
										XSelectionRequestEvent *req;
										XEvent respond;

										req=&(e->xselectionrequest);
										sdl.mutexP(sdl_copybuf_mutex);
										if(sdl_copybuf==NULL) {
											respond.xselection.property=None;
										}
										else {
											if(req->target==XA_STRING) {
												sdl_x11.XChangeProperty(req->display, req->requestor, req->property, XA_STRING, 8, PropModeReplace, (unsigned char *)sdl_copybuf, strlen(sdl_copybuf));
												respond.xselection.property=req->property;
											}
											else
												respond.xselection.property=None;
										}
										sdl.mutexV(sdl_copybuf_mutex);
										respond.xselection.type=SelectionNotify;
										respond.xselection.display=req->display;
										respond.xselection.requestor=req->requestor;
										respond.xselection.selection=req->selection;
										respond.xselection.target=req->target;
										respond.xselection.time=req->time;
										sdl_x11.XSendEvent(req->display,req->requestor,0,0,&respond);
										break;
								}
							}	/* switch */
						}	/* usingx11 */
#endif

					/* Ignore this stuff */
					case SDL_JOYAXISMOTION:
					case SDL_JOYBALLMOTION:
					case SDL_JOYHATMOTION:
					case SDL_JOYBUTTONDOWN:
					case SDL_JOYBUTTONUP:
					default:
						break;
				}
			}
		}
	}
	return(0);
}

int sdl_initciolib(int mode)
{
	if(init_sdl_video()) {
		fprintf(stderr,"SDL Video Initialization Failed\n");
		return(-1);
	}
	sdl_key_pending=sdl.SDL_CreateSemaphore(0);
	sdl_ufunc_ret=sdl.SDL_CreateSemaphore(0);
	sdl_ufunc_rec=sdl.SDL_CreateSemaphore(0);
	sdl_ufunc_mtx=sdl.SDL_CreateMutex();
	sdl_headlock=sdl.SDL_CreateMutex();
	newrect_mutex=sdl.SDL_CreateMutex();
	win_mutex=sdl.SDL_CreateMutex();
	sdl_keylock=sdl.SDL_CreateMutex();
	bitmap_init_mutex=sdl.SDL_CreateMutex();
#if !defined(NO_X) && defined(__unix__)
	sdl_pastebuf_set=sdl.SDL_CreateSemaphore(0);
	sdl_pastebuf_copied=sdl.SDL_CreateSemaphore(0);
	sdl_copybuf_mutex=sdl.SDL_CreateMutex();
#endif
	run_sdl_drawing_thread(sdl_video_event_thread, exit_sdl_con);
	return(sdl_init(mode));
}
