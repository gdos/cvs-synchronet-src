/* misc.cpp */

/* Synchronet miscellaneous utility-type routines (exported) */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"
#include "crc32.h"

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	int file,share,count=0;

    if(access&O_DENYNONE) {
        share=SH_DENYNO;
        access&=~O_DENYNONE; }
    else if(access==O_RDONLY) share=SH_DENYWR;
    else share=SH_DENYRW;
    while(((file=_sopen(str,O_BINARY|access,share,S_IWRITE|S_IREAD))==-1)
        && errno==EACCES && count++<LOOP_NOPEN)
        if(count>10)
            mswait(55);
    return(file);
}

/****************************************************************************/
/* Returns the number of characters in 'str' not counting ctrl-ax codes		*/
/* or the null terminator													*/
/****************************************************************************/
int bstrlen(char *str)
{
	int i=0;

	while(*str) {
		if(*str==1) /* ctrl-a */
			str++;
		else
			i++;
		if(!(*str)) break;
		str++; }
	return(i);
}

void strip_ctrl(char *str)
{
	char tmp[1024];
	int i,j,k;

	k=strlen(str);
	for(i=j=0;i<k;i++)
		if(str[i]==1)  /* Ctrl-a */
			i++;
		else if(j && str[i]<=SP && tmp[j-1]==SP)
			continue;
		else if(i && !isalnum(str[i]) && str[i]==str[i-1])
			continue;
		else if((uchar)str[i]>=SP)
			tmp[j++]=str[i];
		else if(str[i]==TAB || (str[i]==CR && str[i+1]==LF))
			tmp[j++]=SP;
	tmp[j]=0;
	strcpy(str,tmp);
}

void strip_exascii(char *str)
{
	char tmp[1024];
	int i,j,k;

	k=strlen(str);
	for(i=j=0;i<k;i++)
		if(!(str[i]&0x80))
			tmp[j++]=str[i];
	tmp[j]=0;
	strcpy(str,tmp);
}

/****************************************************************************/
/* Returns in 'string' a character representation of the number in l with   */
/* commas.																	*/
/****************************************************************************/
char *ultoac(ulong l, char *string)
{
	char str[256];
	char i,j,k;

	ultoa(l,str,10);
	i=strlen(str)-1;
	j=i/3+1+i;
	string[j--]=0;
	for(k=1;i>-1;k++) {
		string[j--]=str[i--];
		if(j>0 && !(k%3))
			string[j--]=','; }
	return(string);
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
void truncsp(char *str)
{
	uint c;

#if 0  /* no longer terminates at first tab OCT-09-2000 rswindell */
	str[strcspn(str,"\t")]=0;
#endif
	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
	str[c]=0;
}

/****************************************************************************/
/* Puts a backslash on path strings 										*/
/****************************************************************************/
void backslash(char *str)
{
    int i;

	i=strlen(str);
	if(i && str[i-1]!='\\' && str[i-1]!='/') {
		str[i]='\\'; str[i+1]=0; }
}

/****************************************************************************/
/* Puts a backslash on path strings if not just a drive letter and colon	*/
/****************************************************************************/
void backslashcolon(char *str)
{
    int i;

	i=strlen(str);
	if(i && str[i-1]!='\\' && str[i-1]!='/' && str[i-1]!=':') {
		str[i]='\\'; str[i+1]=0; }
}

/****************************************************************************/
/* Updates 16-bit "rcrc" with character 'ch'                                */
/****************************************************************************/
void ucrc16(uchar ch, ushort *rcrc) 
{
	ushort i, cy;
    uchar nch=ch;
 
	for (i=0; i<8; i++) {
		cy=*rcrc & 0x8000;
		*rcrc<<=1;
		if (nch & 0x80) *rcrc |= 1;
		nch<<=1;
		if (cy) *rcrc ^= 0x1021; }
}

/****************************************************************************/
/* Returns CRC-16 of string (not including terminating NULL)				*/
/****************************************************************************/
ushort crc16(char *str)
{
	int 	i=0;
	ushort	crc=0;

	ucrc16(0,&crc);
	while(str[i])
		ucrc16(str[i++],&crc);
	ucrc16(0,&crc);
	ucrc16(0,&crc);
	return(crc);
}

/****************************************************************************/
/* Returns CRC-32 of string (not including terminating NULL)				*/
/****************************************************************************/
ulong crc32(char *buf, ulong len)
{
	ulong l,crc=0xffffffff;

	for(l=0;l<len;l++)
		crc=ucrc32(buf[l],crc);
	return(~crc);
}

/****************************************************************************/
/* Compares pointers to pointers to char. Used in conjuction with qsort()   */
/****************************************************************************/
int pstrcmp(char **str1, char **str2)
{
	return(strcmp(*str1,*str2));
}

/****************************************************************************/
/* Returns the number of characters that are the same between str1 and str2 */
/****************************************************************************/
int strsame(char *str1, char *str2)
{
	int i,j=0;

	for(i=0;str1[i];i++)
		if(str1[i]==str2[i]) j++;
	return(j);
}

#define MV_BUFLEN	4096

/****************************************************************************/
/* Converts when_t.zone into ASCII format                                   */
/****************************************************************************/
char *zonestr(short zone)
{
    static char str[32];

	switch((ushort)zone) {
		case 0:     return("UT");
		case AST:   return("AST");
		case EST:   return("EST");
		case CST:   return("CST");
		case MST:   return("MST");
		case PST:   return("PST");
		case YST:   return("YST");
		case HST:   return("HST");
		case BST:   return("BST");
		case ADT:   return("ADT");
		case EDT:   return("EDT");
		case CDT:   return("CDT");
		case MDT:   return("MDT");
		case PDT:   return("PDT");
		case YDT:   return("YDT");
		case HDT:   return("HDT");
		case BDT:   return("BDT");
		case MID:   return("MID");
		case VAN:   return("VAN");
		case EDM:   return("EDM");
		case WIN:   return("WIN");
		case BOG:   return("BOG");
		case CAR:   return("CAR");
		case RIO:   return("RIO");
		case FER:   return("FER");
		case AZO:   return("AZO");
		case LON:   return("LON");
		case BER:   return("BER");
		case ATH:   return("ATH");
		case MOS:   return("MOS");
		case DUB:   return("DUB");
		case KAB:   return("KAB");
		case KAR:   return("KAR");
		case BOM:   return("BOM");
		case KAT:   return("KAT");
		case DHA:   return("DHA");
		case BAN:   return("BAN");
		case HON:   return("HON");
		case TOK:   return("TOK");
		case SYD:   return("SYD");
		case NOU:   return("NOU");
		case WEL:   return("WEL");
		}

	sprintf(str,"%02d:%02u",zone/60,zone<0 ? (-zone)%60 : zone%60);
	return(str);
}

/****************************************************************************/
/* Checks the disk drive for the existence of a file. Returns 1 if it       */
/* exists, 0 if it doesn't.                                                 */
/****************************************************************************/
BOOL fexist(char *filespec)
{
	long	handle;
    struct _finddata_t f;

	if((handle=_findfirst(filespec,&f))==-1)
		return(FALSE);
	_findclose(handle);
	if(f.attrib&_A_SUBDIR)
		return(FALSE);
	return(TRUE);
}

/****************************************************************************/
/* Returns the length of the file in 'filespec'                             */
/****************************************************************************/
long flength(char *filespec)
{
	long	handle;
    struct _finddata_t f;

	if((handle=_findfirst(filespec,&f))==-1)
		return(-1L);
	_findclose(handle);
	return(f.size);
}

/****************************************************************************/
/* Returns the time/date of the file in 'filespec' in time_t (unix) format  */
/****************************************************************************/
long fdate(char *filespec)
{
    long	handle;
	struct _finddata_t f;

	if((handle=_findfirst(filespec,&f))==-1)
		return(-1L);
	_findclose(handle);
	return(f.time_write);
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char *faddrtoa(faddr_t addr)
{
    static char str[25];
    char point[25];

	sprintf(str,"%u:%u/%u",addr.zone,addr.net,addr.node);
	if(addr.point) {
		sprintf(point,".%u",addr.point);
		strcat(str,point); }
	return(str);
}

/****************************************************************************/
/* Returns string for 2 digit hex+ numbers up to 575						*/
/****************************************************************************/
char *hexplus(uint num, char *str)
{
	sprintf(str,"%03x",num);
	str[0]=num/0x100 ? 'f'+(num/0x10)-0xf : str[1];
	str[1]=str[2];
	str[2]=0;
	return(str);
}

uint hptoi(char *str)
{
	char tmp[128];
	uint i;

	if(!str[1] || toupper(str[0])<='F')
		return(ahtoul(str));
	strcpy(tmp,str);
	tmp[0]='F';
	i=ahtoul(tmp)+((toupper(str[0])-'F')*0x10);
	return(i);
}

/****************************************************************************/
/* Converts an ASCII Hex string into an ulong                               */
/****************************************************************************/
ulong ahtoul(char *str)
{
    ulong l,val=0;

	while((l=(*str++)|0x20)!=0x20)
		val=(l&0xf)+(l>>6&1)*9+val*16;
	return(val);
}
#ifndef __BORLANDC__
int random(int n)
{
	float f;

	if(n<2)
		return(0);
	f=(float)rand()/(float)RAND_MAX;

	return((int)(n*f));
}
#endif


typedef BOOL(WINAPI * GetDiskFreeSpaceEx_t)(LPCTSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER); 

ulong getfreediskspace(char* path)
{
#ifdef _WIN32
	HINSTANCE		hK32;
	DWORD			TotalNumberOfClusters;
	DWORD			NumberOfFreeClusters;
	DWORD			BytesPerSector;
	DWORD			SectorsPerCluster;
	ULARGE_INTEGER	avail;
	ULARGE_INTEGER	size;
	GetDiskFreeSpaceEx_t GetDiskFreeSpaceEx;

	if ((hK32 = LoadLibrary("KERNEL32")) == NULL)
		return(0);

	GetDiskFreeSpaceEx = (GetDiskFreeSpaceEx_t)GetProcAddress(hK32,"GetDiskFreeSpaceEx");
 
	if (GetDiskFreeSpaceEx!=NULL) {	/* Windows 95-OSR2 or later */
		if(!GetDiskFreeSpaceEx(
			path,		// pointer to the directory name
			&avail,		// receives the number of bytes on disk available to the caller
			&size,		// receives the number of bytes on disk
			NULL))		// receives the free bytes on disk
			return(0);

		if(avail.HighPart)
			return(0xffffffff);
		return(avail.LowPart);
	}

	/* Windows 95 (old way), limited to 2GB */
	if(!GetDiskFreeSpace(
		path,					// pointer to root path
		&SectorsPerCluster,		// pointer to sectors per cluster
		&BytesPerSector,		// pointer to bytes per sector
		&NumberOfFreeClusters,	// pointer to number of free clusters
		&TotalNumberOfClusters  // pointer to total number of clusters
		))
		return(0);

	return(NumberOfFreeClusters*SectorsPerCluster*BytesPerSector);
#else
	#warning OS-specific code needed here
	return(0);
#endif
}
