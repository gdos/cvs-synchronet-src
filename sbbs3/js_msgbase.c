/* Synchronet JavaScript "MsgBase" Object */

/* $Id: js_msgbase.c,v 1.222 2018/09/06 02:21:10 rswindell Exp $ */
// vi: tabstop=4

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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
#include "js_request.h"
#include "userdat.h"

#ifdef JAVASCRIPT

typedef struct
{
	smb_t	smb;
	int		smb_result;
	BOOL	debug;

} private_t;

typedef struct
{
	private_t	*p;
	BOOL		expand_fields;
	smbmsg_t	msg;

} privatemsg_t;

static const char* getprivate_failure = "line %d %s %s JS_GetPrivate failed";

JSBool JS_ValueToUint32(JSContext *cx, jsval v, uint32 *ip)
{
	jsdouble d;

	if(!JS_ValueToNumber(cx, v, &d))
		return JS_FALSE;
	*ip = (uint32)d;
	return JS_TRUE;
}

/* Destructor */

static void js_finalize_msgbase(JSContext *cx, JSObject *obj)
{
	private_t* p;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return;

	if(SMB_IS_OPEN(&(p->smb)))
		smb_close(&(p->smb));

	free(p);

	JS_SetPrivate(cx, obj, NULL);
}

/* Methods */

static JSBool
js_open(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount	rc;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if(p->smb.subnum==INVALID_SUB 
		&& strchr(p->smb.file,'/')==NULL
		&& strchr(p->smb.file,'\\')==NULL) {
		JS_ReportError(cx,"Unrecognized msgbase code: %s",p->smb.file);
		return JS_TRUE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	if((p->smb_result=smb_open(&(p->smb)))!=SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return JS_TRUE;
}


static JSBool
js_close(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount	rc;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	smb_close(&(p->smb));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static BOOL parse_recipient_object(JSContext* cx, private_t* p, JSObject* hdr, smbmsg_t* msg)
{
	char*		cp=NULL;
	size_t		cp_sz=0;
	char		to[256];
	ushort		nettype=NET_UNKNOWN;
	ushort		agent;
	int32		i32;
	jsval		val;
	scfg_t*		scfg;

	scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	
	if(JS_GetProperty(cx, hdr, "to", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"to\" string in recipient object");
			return(FALSE);
		}
	} else {
		if(p->smb.status.attr&SMB_EMAIL) {	/* e-mail */
			JS_ReportError(cx, "\"to\" property not included in email recipient object");
			return(FALSE);					/* "to" property required */
		}
		cp=strdup("All");
	}

	if((p->smb_result=smb_hfield_str(msg, RECIPIENT, cp))!=SMB_SUCCESS) {
		JS_ReportError(cx, "Error %d adding RECIPIENT field to message header", p->smb_result);
		free(cp);
		return(FALSE);
	}
	if(!(p->smb.status.attr&SMB_EMAIL)) {
		SAFECOPY(to,cp);
		strlwr(to);
		msg->idx.to=crc16(to,0);
	}

	if(JS_GetProperty(cx, hdr, "to_ext", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"to_ext\" string in recipient object");
			return(FALSE);
		}
		if((p->smb_result=smb_hfield_str(msg, RECIPIENTEXT, cp))!=SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding RECIPIENTEXT field to message header", p->smb_result);
			return(FALSE);
		}
		if(p->smb.status.attr&SMB_EMAIL)
			msg->idx.to=atoi(cp);
	}

	if(JS_GetProperty(cx, hdr, "to_org", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"to_org\" string in recipient object");
			return(FALSE);
		}
		if((p->smb_result=smb_hfield_str(msg, RECIPIENTORG, cp))!=SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding RECIPIENTORG field to message header", p->smb_result);
			return(FALSE);
		}
	}

	if(JS_GetProperty(cx, hdr, "to_net_type", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32)) {
			free(cp);
			return(FALSE);
		}
		nettype=(ushort)i32;
	}

	if(JS_GetProperty(cx, hdr, "to_net_addr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"to_net_addr\" string in recipient object");
			return(FALSE);
		}
		if((p->smb_result=smb_hfield_netaddr(msg, RECIPIENTNETADDR, cp, &nettype))!=SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding RECIPIENTADDR field to message header", p->smb_result);
			return(FALSE);
		}
	}
	free(cp);

	if(nettype!=NET_UNKNOWN && nettype!=NET_NONE) {
		if(p->smb.status.attr&SMB_EMAIL) {
			if(nettype==NET_QWK && msg->idx.to==0) {
				char fulladdr[128];
				msg->idx.to = qwk_route(scfg, msg->to_net.addr, fulladdr, sizeof(fulladdr)-1);
				if(fulladdr[0]==0) {
					JS_ReportError(cx, "Unrouteable QWKnet \"to_net_addr\" (%s) in recipient object"
						,msg->to_net.addr);
					return(FALSE);
				}
				if((p->smb_result=smb_hfield_str(msg, RECIPIENTNETADDR, fulladdr))!=SMB_SUCCESS) {
					JS_ReportError(cx, "Error %d adding RECIPIENTADDR field to message header"
						,p->smb_result);
					return(FALSE);
				}
				if(msg->idx.to != 0) {
					char ext[32];
					sprintf(ext,"%u",msg->idx.to);
					if((p->smb_result=smb_hfield_str(msg, RECIPIENTEXT, ext))!=SMB_SUCCESS) {
						JS_ReportError(cx, "Error %d adding RECIPIENTEXT field to message header"
							,p->smb_result);
						return(FALSE);
					}
				}
			} else
				msg->idx.to=0;
		}
		if((p->smb_result=smb_hfield_bin(msg, RECIPIENTNETTYPE, nettype))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding RECIPIENTNETTYPE field to message header", p->smb_result);
			return(FALSE);
		}
	}

	if(JS_GetProperty(cx, hdr, "to_agent", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			return FALSE;
		agent=(ushort)i32;
		if((p->smb_result=smb_hfield_bin(msg, RECIPIENTAGENT, agent))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding RECIPIENTAGENT field to message header", p->smb_result);
			return(FALSE);
		}
	}

	return(TRUE);
}

static BOOL parse_header_object(JSContext* cx, private_t* p, JSObject* hdr, smbmsg_t* msg
								,BOOL recipient)
{
	char*		cp=NULL;
	size_t		cp_sz=0;
	char		from[256];
	ushort		nettype=NET_UNKNOWN;
	ushort		type;
	ushort		agent;
	int32		i32;
	uint32		u32;
	jsval		val;
	JSObject*	array;
	JSObject*	field;
	jsuint		i,len;

	if(hdr==NULL) {
		JS_ReportError(cx, "NULL header object");
		goto err;
	}

	if(recipient && !parse_recipient_object(cx,p,hdr,msg))
		goto err;

	if(msg->hdr.type != SMB_MSG_TYPE_BALLOT) {
		/* Required Header Fields */
		if(JS_GetProperty(cx, hdr, "subject", &val) && !JSVAL_NULL_OR_VOID(val)) {
			JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
			HANDLE_PENDING(cx, cp);
			if(cp==NULL) {
				JS_ReportError(cx, "Invalid \"subject\" string in header object");
				goto err;
			}
		} else
			cp=strdup("");

		if((p->smb_result=smb_hfield_str(msg, SUBJECT, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SUBJECT field to message header", p->smb_result);
			goto err;
		}
		msg->idx.subj=smb_subject_crc(cp);
	}
	if(JS_GetProperty(cx, hdr, "from", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"from\" string in header object");
			goto err;
		}
	} else {
		JS_ReportError(cx, "\"from\" property required in header");
		goto err;	/* "from" property required */
	}
	if((p->smb_result=smb_hfield_str(msg, SENDER, cp))!=SMB_SUCCESS) {
		JS_ReportError(cx, "Error %d adding SENDER field to message header", p->smb_result);
		goto err;
	}
	if(!(p->smb.status.attr&SMB_EMAIL) && msg->hdr.type != SMB_MSG_TYPE_BALLOT) {
		SAFECOPY(from,cp);
		strlwr(from);
		msg->idx.from=crc16(from,0);
	}

	/* Optional Header Fields */
	if(JS_GetProperty(cx, hdr, "from_ext", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"from_ext\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, SENDEREXT, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDEREXT field to message header", p->smb_result);
			goto err;
		}
		if(p->smb.status.attr&SMB_EMAIL)
			msg->idx.from=atoi(cp);
	}

	if(JS_GetProperty(cx, hdr, "from_org", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"from_org\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, SENDERORG, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERORG field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "from_net_type", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32)) {
			goto err;
		}
		nettype=(ushort)i32;
	}

	if(JS_GetProperty(cx, hdr, "from_net_addr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"from_net_addr\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_netaddr(msg, SENDERNETADDR, cp, &nettype))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERNETADDR field to message header", p->smb_result);
			goto err;
		}
	}
	
	if(nettype!=NET_UNKNOWN && nettype!=NET_NONE) {
		if(p->smb.status.attr&SMB_EMAIL)
			msg->idx.from=0;
		if((p->smb_result=smb_hfield_bin(msg, SENDERNETTYPE, nettype))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERNETTYPE field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "from_agent", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		agent=(ushort)i32;
		if((p->smb_result=smb_hfield_bin(msg, SENDERAGENT, agent))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERAGENT field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "from_ip_addr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"from_ip_addr\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, SENDERIPADDR, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERIPADDR field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "from_host_name", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"from_host_name\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, SENDERHOSTNAME, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERHOSTNAME field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "from_protocol", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"from_protocol\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, SENDERPROTOCOL, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERPROTOCOL field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "from_port", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"from_port\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, SENDERPORT, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERPORT field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "sender_userid", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"sender_userid\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, SENDERUSERID, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERUSERID field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "sender_server", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"sender_server\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, SENDERSERVER, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERSERVER field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "sender_time", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"sender_time\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, SENDERTIME, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERTIME field to message header", p->smb_result);
			goto err;
		}
	}
	
	if(JS_GetProperty(cx, hdr, "replyto", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"replyto\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, REPLYTO, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTO field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "replyto_ext", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"replyto_ext\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, REPLYTOEXT, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTOEXT field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "replyto_org", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"replyto_org\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, REPLYTOORG, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTOORG field to message header", p->smb_result);
			goto err;
		}
	}

	nettype=NET_UNKNOWN;
	if(JS_GetProperty(cx, hdr, "replyto_net_type", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		nettype=(ushort)i32;
	}
	if(JS_GetProperty(cx, hdr, "replyto_net_addr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"replyto_net_addr\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_netaddr(msg, REPLYTONETADDR, cp, &nettype))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTONETADDR field to message header", p->smb_result);
			goto err;
		}
	}
	if(nettype!=NET_UNKNOWN && nettype!=NET_NONE) {
		if((p->smb_result=smb_hfield_bin(msg, REPLYTONETTYPE, nettype))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTONETTYPE field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "replyto_agent", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		agent=(ushort)i32;
		if((p->smb_result=smb_hfield_bin(msg, REPLYTOAGENT, agent))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTOAGENT field to message header", p->smb_result);
			goto err;
		}
	}

	/* RFC822 headers */
	if(JS_GetProperty(cx, hdr, "id", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"id\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, RFC822MSGID, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding RFC822MSGID field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "reply_id", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"reply_id\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, RFC822REPLYID, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding RFC822REPLYID field to message header", p->smb_result);
			goto err;
		}
	}

	/* SMTP headers */
	if(JS_GetProperty(cx, hdr, "reverse_path", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"reverse_path\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, SMTPREVERSEPATH, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SMTPREVERSEPATH field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "forward_path", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"forward_path\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, SMTPFORWARDPATH, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SMTPFORWARDPATH field to message header", p->smb_result);
			goto err;
		}
	}

	/* USENET headers */
	if(JS_GetProperty(cx, hdr, "path", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"path\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, USENETPATH, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding USENETPATH field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "newsgroups", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"newsgroups\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, USENETNEWSGROUPS, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding USENETNEWSGROUPS field to message header", p->smb_result);
			goto err;
		}
	}

	/* FTN headers */
	if(JS_GetProperty(cx, hdr, "ftn_msgid", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"ftn_msgid\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, FIDOMSGID, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOMSGID field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "ftn_reply", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"ftn_reply\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, FIDOREPLYID, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOREPLYID field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "ftn_area", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"ftn_area\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, FIDOAREA, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOAREA field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "ftn_flags", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"ftn_flags\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, FIDOFLAGS, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOFLAGS field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "ftn_pid", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"ftn_pid\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, FIDOPID, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOPID field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "ftn_tid", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"ftn_tid\" string in header object");
			goto err;
		}
		if((p->smb_result=smb_hfield_str(msg, FIDOTID, cp))!=SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOTID field to message header", p->smb_result);
			goto err;
		}
	}

	if(JS_GetProperty(cx, hdr, "date", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid \"date\" string in header object");
			goto err;
		}
		msg->hdr.when_written=rfc822date(cp);
	}

	/* Numeric Header Fields */
	if(JS_GetProperty(cx, hdr, "attr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.attr=(ushort)i32;
		msg->idx.attr=msg->hdr.attr;
	}
	if(JS_GetProperty(cx, hdr, "votes", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.votes=(ushort)i32;
		if(msg->hdr.type == SMB_MSG_TYPE_BALLOT)
			msg->idx.votes=msg->hdr.votes;
	}
	if(JS_GetProperty(cx, hdr, "auxattr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToUint32(cx,val,&u32))
			goto err;
		msg->hdr.auxattr=u32;
	}
	if(JS_GetProperty(cx, hdr, "netattr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.netattr=i32;
	}
	if(JS_GetProperty(cx, hdr, "when_written_time", &val) && !JSVAL_NULL_OR_VOID(val))  {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.when_written.time=i32;
	}
	if(JS_GetProperty(cx, hdr, "when_written_zone", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.when_written.zone=(short)i32;
	}
	if(JS_GetProperty(cx, hdr, "when_imported_time", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.when_imported.time=i32;
	}
	if(JS_GetProperty(cx, hdr, "when_imported_zone", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.when_imported.zone=(short)i32;
	}

	if(JS_GetProperty(cx, hdr, "thread_id", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.thread_id=i32;
	}
	if((JS_GetProperty(cx, hdr, "thread_orig", &val) && (!JSVAL_NULL_OR_VOID(val)))
			|| (JS_GetProperty(cx, hdr, "thread_back", &val) && !JSVAL_NULL_OR_VOID(val))) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.thread_back=i32;
	}
	if(JS_GetProperty(cx, hdr, "thread_next", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.thread_next=i32;
	}
	if(JS_GetProperty(cx, hdr, "thread_first", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.thread_first=i32;
	}

	if(JS_GetProperty(cx, hdr, "field_list", &val) && JSVAL_IS_OBJECT(val)) {
		array=JSVAL_TO_OBJECT(val);
		len=0;
		if(!JS_GetArrayLength(cx, array, &len)) {
			JS_ReportError(cx, "Invalid \"field_list\" array in header object");
			goto err;
		}

		for(i=0;i<len;i++) {
			if(!JS_GetElement(cx, array, i, &val))
				continue;
			if(!JSVAL_IS_OBJECT(val))
				continue;
			field=JSVAL_TO_OBJECT(val);
			if(!JS_GetProperty(cx, field, "type", &val))
				continue;
			if(JSVAL_IS_STRING(val)) {
				JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
				HANDLE_PENDING(cx, cp);
				type=smb_hfieldtypelookup(cp);
			}
			else {
				if(!JS_ValueToInt32(cx,val,&i32))
					goto err;
				type=(ushort)i32;
			}
			if(!JS_GetProperty(cx, field, "data", &val))
				continue;
			JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
			HANDLE_PENDING(cx, cp);
			if(cp==NULL) {
				JS_ReportError(cx, "Invalid data string in \"field_list\" array");
				goto err;
			}
			if((p->smb_result=smb_hfield_str(msg, type, cp))!=SMB_SUCCESS) {
				JS_ReportError(cx, "Error %d adding field (type %02Xh) to message header", p->smb_result, type);
				goto err;
			}
		}
	}

	if(msg->hdr.number==0 && JS_GetProperty(cx, hdr, "number", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToInt32(cx,val,&i32))
			goto err;
		msg->hdr.number=i32;
	}

	if(cp)
		free(cp);
	return(TRUE);

err:
	if(cp)
		free(cp);
	return(FALSE);
}

/* obj must've been previously returned from get_msg_header() */
BOOL DLLCALL js_ParseMsgHeaderObject(JSContext* cx, JSObject* obj, smbmsg_t* msg)
{
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(FALSE);
	}

	if(!parse_header_object(cx, p, obj, msg, /* recipient */ TRUE)) {
		smb_freemsgmem(msg);
		return(FALSE);
	}

	return(TRUE);
}

static BOOL msg_offset_by_id(private_t* p, char* id, int32_t* offset)
{
	smbmsg_t msg;

	if((p->smb_result=smb_getmsgidx_by_msgid(&(p->smb),&msg,id))!=SMB_SUCCESS)
		return(FALSE);

	*offset = msg.offset;
	return(TRUE);
}

static JSBool
js_get_msg_index(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		n;
	jsval		val;
	smbmsg_t	msg;
	JSObject*	idxobj;
	JSBool		by_offset=JS_FALSE;
	JSBool		include_votes=JS_FALSE;
	private_t*	p;
	jsrefcount	rc;
	JSObject*	proto;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	memset(&msg,0,sizeof(msg));

	n=0;
	if(n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		by_offset = JSVAL_TO_BOOLEAN(argv[n++]);

	for(;n<argc;n++) {
		if(JSVAL_IS_BOOLEAN(argv[n]))
			include_votes = JSVAL_TO_BOOLEAN(argv[n]);
		else if(JSVAL_IS_NUMBER(argv[n])) {
			if(by_offset) {							/* Get by offset */
				if(!JS_ValueToInt32(cx, argv[n], (int32*)&msg.offset))
					return JS_FALSE;
			}
			else {									/* Get by number */
				if(!JS_ValueToInt32(cx, argv[n], (int32*)&msg.hdr.number))
					return JS_FALSE;
			}
		}
	}

	rc=JS_SUSPENDREQUEST(cx);
	p->smb_result = smb_getmsgidx(&(p->smb), &msg);
	JS_RESUMEREQUEST(cx, rc);
	if(p->smb_result != SMB_SUCCESS)
		return JS_TRUE;

	if(!include_votes && (msg.idx.attr&MSG_VOTE))
		return JS_TRUE;

	if(JS_GetProperty(cx, JS_GetGlobalObject(cx), "MsgBase", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx,val,&proto);
		if(JS_GetProperty(cx, proto, "IndexPrototype", &val) && !JSVAL_NULL_OR_VOID(val))
			JS_ValueToObject(cx,val,&proto);
		else
			proto=NULL;
	}
	else
		proto=NULL;

	if((idxobj=JS_NewObject(cx,NULL,proto,obj))==NULL)
		return JS_TRUE;

	val=UINT_TO_JSVAL(msg.idx.number);
	JS_DefineProperty(cx, idxobj, "number"	,val
		,NULL,NULL,JSPROP_ENUMERATE);

	if(msg.idx.attr&MSG_VOTE && !(msg.idx.attr&MSG_POLL)) {
		val=UINT_TO_JSVAL(msg.idx.votes);
		JS_DefineProperty(cx, idxobj, "votes"	,val
			,NULL,NULL,JSPROP_ENUMERATE);

		val=UINT_TO_JSVAL(msg.idx.remsg);
		JS_DefineProperty(cx, idxobj, "remsg"	,val
			,NULL,NULL,JSPROP_ENUMERATE);
	} else {	/* normal message */
		val=UINT_TO_JSVAL(msg.idx.to);
		JS_DefineProperty(cx, idxobj, "to"		,val
			,NULL,NULL,JSPROP_ENUMERATE);

		val=UINT_TO_JSVAL(msg.idx.from);
		JS_DefineProperty(cx, idxobj, "from"	,val
			,NULL,NULL,JSPROP_ENUMERATE);

		val=UINT_TO_JSVAL(msg.idx.subj);
		JS_DefineProperty(cx, idxobj, "subject"	,val
			,NULL,NULL,JSPROP_ENUMERATE);
	}
	val=UINT_TO_JSVAL(msg.idx.attr);
	JS_DefineProperty(cx, idxobj, "attr"	,val
		,NULL,NULL,JSPROP_ENUMERATE);

	val=UINT_TO_JSVAL(msg.offset);
	JS_DefineProperty(cx, idxobj, "offset"	,val
		,NULL,NULL,JSPROP_ENUMERATE);

	val=UINT_TO_JSVAL(msg.idx.time);
	JS_DefineProperty(cx, idxobj, "time"	,val
		,NULL,NULL,JSPROP_ENUMERATE);

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(idxobj));

	return JS_TRUE;
}

#define LAZY_INTEGER(PropName, PropValue, flags) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		if(name) free(name); \
		v=INT_TO_JSVAL((PropValue)); \
		JS_DefineProperty(cx, obj, (PropName), v, NULL,NULL,flags); \
		if(name) return JS_TRUE; \
	}

#define LAZY_UINTEGER(PropName, PropValue, flags) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		if(name) free(name); \
		v=UINT_TO_JSVAL((PropValue)); \
		JS_DefineProperty(cx, obj, (PropName), v, NULL,NULL,flags); \
		if(name) return JS_TRUE; \
	}

#define LAZY_UINTEGER_EXPAND(PropName, PropValue, flags) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		if(name) free(name); \
		if(p->expand_fields || (PropValue)) { \
			v=UINT_TO_JSVAL((PropValue)); \
			JS_DefineProperty(cx, obj, (PropName), v, NULL,NULL,flags); \
			if(name) return JS_TRUE; \
		} \
		else if(name) return JS_TRUE; \
	}

#define LAZY_UINTEGER_COND(PropName, Condition, PropValue, flags) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		if(name) free(name); \
		if(Condition) { \
			v=UINT_TO_JSVAL((uint32_t)(PropValue)); \
			JS_DefineProperty(cx, obj, (PropName), v, NULL,NULL,flags); \
			if(name) return JS_TRUE; \
		} \
		else if(name) return JS_TRUE; \
	}

#define LAZY_STRING(PropName, PropValue, flags) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		if(name) free(name); \
		if((js_str=JS_NewStringCopyZ(cx, (PropValue)))!=NULL) { \
			JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, flags); \
			if(name) return JS_TRUE; \
		} \
		else if(name) return JS_TRUE; \
	}

#define LAZY_STRING_TRUNCSP(PropName, PropValue, flags) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		if(name) free(name); \
		if((js_str=JS_NewStringCopyZ(cx, truncsp(PropValue)))!=NULL) { \
			JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, flags); \
			if(name) return JS_TRUE; \
		} \
		else if(name) return JS_TRUE; \
	}

#define LAZY_STRING_COND(PropName, Condition, PropValue, flags) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		if(name) free(name); \
		if((Condition) && (js_str=JS_NewStringCopyZ(cx, (PropValue)))!=NULL) { \
			JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, flags); \
			if(name) return JS_TRUE; \
		} \
		else if(name) return JS_TRUE; \
	}

#define LAZY_STRING_TRUNCSP_NULL(PropName, PropValue, flags) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		if(name) free(name); \
		if((PropValue) != NULL && (js_str=JS_NewStringCopyZ(cx, truncsp(PropValue)))!=NULL) { \
			JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, flags); \
			if(name) return JS_TRUE; \
		} \
		else if(name) return JS_TRUE; \
	}

static JSBool js_get_msg_header_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char			date[128];
	char			msg_id[256];
	char			reply_id[256];
	char			tmp[128];
	char*			val;
	int				i;
	smbmsg_t		remsg;
	JSObject*		array;
	JSObject*		field;
	JSString*		js_str;
	jsint			items;
	jsval			v;
	privatemsg_t*	p;
	char*			name=NULL;
	jsrefcount		rc;
	scfg_t*			scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}

	/* If we have already enumerated, we're done here... */
	if((p=(privatemsg_t*)JS_GetPrivate(cx,obj))==NULL) {
		if(name) free(name);
		return JS_TRUE;
	}

	if((p->msg).hdr.number==0) { /* No valid message number/id/offset specified */
		if(name) free(name);
		return JS_TRUE;
	}

	LAZY_UINTEGER("number", p->msg.hdr.number, JSPROP_ENUMERATE);
	LAZY_UINTEGER("offset", p->msg.offset, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP("to",p->msg.to, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP("from",p->msg.from, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP("subject",p->msg.subj, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("summary", p->msg.summary, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("to_ext", p->msg.to_ext, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("from_ext", p->msg.from_ext, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("from_org", p->msg.from_org, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("replyto", p->msg.replyto, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("replyto_ext", p->msg.replyto_ext, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("reverse_path", p->msg.reverse_path, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("forward_path", p->msg.forward_path, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("to_agent", p->msg.to_agent, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("from_agent", p->msg.from_agent, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("replyto_agent", p->msg.replyto_agent, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("to_net_type", p->msg.to_net.type, JSPROP_ENUMERATE);
	LAZY_STRING_COND("to_net_addr", p->msg.to_net.type && p->msg.to_net.addr, smb_netaddrstr(&(p->msg).to_net,tmp), JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("from_net_type", p->msg.from_net.type, JSPROP_ENUMERATE);
	/* exception here because p->msg.from_net is NULL */
	LAZY_STRING_COND("from_net_addr", p->msg.from_net.type && p->msg.from_net.addr, smb_netaddrstr(&(p->msg).from_net,tmp), JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("replyto_net_type", p->msg.replyto_net.type, JSPROP_ENUMERATE);
	LAZY_STRING_COND("replyto_net_addr", p->msg.replyto_net.type && p->msg.replyto_net.addr, smb_netaddrstr(&(p->msg).replyto_net,tmp), JSPROP_ENUMERATE);
	LAZY_STRING_COND("from_ip_addr", (val=smb_get_hfield(&(p->msg),SENDERIPADDR,NULL))!=NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("from_host_name", (val=smb_get_hfield(&(p->msg),SENDERHOSTNAME,NULL))!=NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("from_protocol", (val=smb_get_hfield(&(p->msg),SENDERPROTOCOL,NULL))!=NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("from_port", (val=smb_get_hfield(&(p->msg),SENDERPORT,NULL))!=NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("sender_userid", (val=smb_get_hfield(&(p->msg),SENDERUSERID,NULL))!=NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("sender_server", (val=smb_get_hfield(&(p->msg),SENDERSERVER,NULL))!=NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("sender_time", (val=smb_get_hfield(&(p->msg),SENDERTIME,NULL))!=NULL, val, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("forwarded", p->msg.forwarded, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("expiration", p->msg.expiration, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("priority", p->msg.priority, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("cost", p->msg.cost, JSPROP_ENUMERATE);

	/* Fixed length portion of msg header */
	LAZY_UINTEGER("type", p->msg.hdr.type, JSPROP_ENUMERATE);
	LAZY_UINTEGER("version", p->msg.hdr.version, JSPROP_ENUMERATE);
	LAZY_UINTEGER("attr", p->msg.hdr.attr, JSPROP_ENUMERATE);
	LAZY_UINTEGER("auxattr", p->msg.hdr.auxattr, JSPROP_ENUMERATE);
	LAZY_UINTEGER("netattr", p->msg.hdr.netattr, JSPROP_ENUMERATE);
	LAZY_UINTEGER("when_written_time", p->msg.hdr.when_written.time, JSPROP_ENUMERATE);
	LAZY_INTEGER("when_written_zone", p->msg.hdr.when_written.zone, JSPROP_ENUMERATE);
	LAZY_INTEGER("when_written_zone_offset", smb_tzutc(p->msg.hdr.when_written.zone), JSPROP_ENUMERATE|JSPROP_READONLY);
	LAZY_UINTEGER("when_imported_time", p->msg.hdr.when_imported.time, JSPROP_ENUMERATE);
	LAZY_INTEGER("when_imported_zone", p->msg.hdr.when_imported.zone, JSPROP_ENUMERATE);
	LAZY_INTEGER("when_imported_zone_offset", smb_tzutc(p->msg.hdr.when_imported.zone), JSPROP_ENUMERATE|JSPROP_READONLY);
	LAZY_UINTEGER("thread_id", p->msg.hdr.thread_id, JSPROP_ENUMERATE);
	LAZY_UINTEGER("thread_back", p->msg.hdr.thread_back, JSPROP_ENUMERATE);
	LAZY_UINTEGER("thread_orig", p->msg.hdr.thread_back, 0);
	LAZY_UINTEGER("thread_next", p->msg.hdr.thread_next, JSPROP_ENUMERATE);
	LAZY_UINTEGER("thread_first", p->msg.hdr.thread_first, JSPROP_ENUMERATE);
	LAZY_UINTEGER("delivery_attempts", p->msg.hdr.delivery_attempts, JSPROP_ENUMERATE);
	LAZY_UINTEGER("last_downloaded", p->msg.hdr.last_downloaded, JSPROP_ENUMERATE);
	LAZY_UINTEGER("times_downloaded", p->msg.hdr.times_downloaded, JSPROP_ENUMERATE);
	LAZY_UINTEGER("data_length", smb_getmsgdatlen(&(p->msg)), JSPROP_ENUMERATE);
	LAZY_STRING("date", msgdate((p->msg).hdr.when_written,date), JSPROP_ENUMERATE);
	LAZY_UINTEGER("votes", p->msg.hdr.votes, JSPROP_ENUMERATE);

	if(name==NULL || strcmp(name,"reply_id")==0) {
		if(name) free(name);
		/* Reply-ID (References) */
		if((p->msg).reply_id!=NULL)
			val=(p->msg).reply_id;
		else {
			reply_id[0]=0;
			if(p->expand_fields && (p->msg).hdr.thread_back) {
				rc=JS_SUSPENDREQUEST(cx);
				memset(&remsg,0,sizeof(remsg));
				remsg.hdr.number=(p->msg).hdr.thread_back;
				if(smb_getmsgidx(&(p->p->smb), &remsg))
					SAFEPRINTF(reply_id,"<%s>",p->p->smb.last_error);
				else
					get_msgid(scfg,p->p->smb.subnum,&remsg,reply_id,sizeof(reply_id));
				JS_RESUMEREQUEST(cx, rc);
			}
			val=reply_id;
		}
		if(val[0] && (js_str=JS_NewStringCopyZ(cx,truncsp(val)))!=NULL) {
			JS_DefineProperty(cx, obj, "reply_id"
				, STRING_TO_JSVAL(js_str)
				,NULL,NULL,JSPROP_ENUMERATE);
			if (name)
				return JS_TRUE;
		}
		else if (name)
			return JS_TRUE;
	}

	/* Message-ID */
	if(name==NULL || strcmp(name,"id")==0) {
		if(name) free(name);
		if(p->expand_fields || (p->msg).id!=NULL) {
			get_msgid(scfg,p->p->smb.subnum,&(p->msg),msg_id,sizeof(msg_id));
			val=msg_id;
			if((js_str=JS_NewStringCopyZ(cx,truncsp(val)))!=NULL) {
				JS_DefineProperty(cx, obj, "id"
					,STRING_TO_JSVAL(js_str)
					,NULL,NULL,JSPROP_ENUMERATE);
				if (name)
					return JS_TRUE;
			}
			else if (name)
				return JS_TRUE;
		}
		else if (name)
			return JS_TRUE;
	}

	/* USENET Fields */
	LAZY_STRING_TRUNCSP_NULL("path", p->msg.path, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("newsgroups", p->msg.newsgroups, JSPROP_ENUMERATE);

	/* FidoNet Header Fields */
	LAZY_STRING_TRUNCSP_NULL("ftn_msgid", p->msg.ftn_msgid, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_reply", p->msg.ftn_reply, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_pid", p->msg.ftn_pid, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_tid", p->msg.ftn_tid, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_area", p->msg.ftn_area, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_flags", p->msg.ftn_flags, JSPROP_ENUMERATE);

	if(name==NULL || strcmp(name,"field_list")==0) {
		if(name) free(name);
		/* Create hdr.field_list[] with repeating header fields (including type and data) */
		if((array=JS_NewArrayObject(cx,0,NULL))!=NULL) {
			JS_DefineProperty(cx,obj,"field_list",OBJECT_TO_JSVAL(array)
				,NULL,NULL,JSPROP_ENUMERATE);
			items=0;
			for(i=0;i<(p->msg).total_hfields;i++) {
				switch((p->msg).hfield[i].type) {
					case SMB_COMMENT:
					case SMB_POLL_ANSWER:
					case SMB_CARBONCOPY:
					case SMB_GROUP:
					case FILEATTACH:
					case DESTFILE:
					case FILEATTACHLIST:
					case DESTFILELIST:
					case FILEREQUEST:
					case FILEPASSWORD:
					case FILEREQUESTLIST:
					case FILEPASSWORDLIST:
					case FIDOCTRL:
					case FIDOSEENBY:
					case FIDOPATH:
					case RFC822HEADER:
					case UNKNOWNASCII:
						/* only support these header field types */
						break;
					default:
						/* dupe or possibly binary header field */
						continue;
				}
				if((field=JS_NewObject(cx,NULL,NULL,array))==NULL)
					continue;
				JS_DefineProperty(cx,field,"type"
					,INT_TO_JSVAL((p->msg).hfield[i].type)
					,NULL,NULL,JSPROP_ENUMERATE);
				if((js_str=JS_NewStringCopyN(cx,(p->msg).hfield_dat[i],(p->msg).hfield[i].length))==NULL)
					break;
				JS_DefineProperty(cx,field,"data"
					,STRING_TO_JSVAL(js_str)
					,NULL,NULL,JSPROP_ENUMERATE);
				JS_DefineElement(cx,array,items,OBJECT_TO_JSVAL(field)
					,NULL,NULL,JSPROP_ENUMERATE);
				items++;
			}
			if (name)
				return JS_TRUE;
		}
		else if (name)
			return JS_TRUE;
	}

	if(name==NULL || strcmp(name, "can_read")==0) {
		if(name) free(name);
		v=BOOLEAN_TO_JSVAL(JS_FALSE);

		do {
			client_t	*client=NULL;
			user_t		*user=NULL;
			jsval		cov;
			ushort		aliascrc,namecrc,sysop=crc16("sysop",0);

			/* dig a client object out of the global object */
			JS_GetProperty(cx, JS_GetGlobalObject(cx), "client", &cov);
			if(JSVAL_IS_OBJECT(cov)) {
				JSObject *obj = JSVAL_TO_OBJECT(cov);
				JSClass	*cl;

				if((cl=JS_GetClass(cx,obj))!=NULL && strcmp(cl->name,"Client")==0)
					client=JS_GetPrivate(cx,obj);
			}
			
			/* dig a user object out of the global object */
			JS_GetProperty(cx, JS_GetGlobalObject(cx), "user", &cov);
			if(JSVAL_IS_OBJECT(cov)) {
				JSObject *obj = JSVAL_TO_OBJECT(cov);
				JSClass	*cl;

				if((cl=JS_GetClass(cx,obj))!=NULL && strcmp(cl->name,"User")==0) {
					user=*(user_t **)(JS_GetPrivate(cx,obj));
					namecrc=crc16(user->name, 0);
					aliascrc=crc16(user->alias, 0);
				}
			}

			if(p->msg.idx.attr&MSG_DELETE) {		/* Pre-flagged */
				if(!(scfg->sys_misc&SM_SYSVDELM)) /* Noone can view deleted msgs */
					break;
				if(!(scfg->sys_misc&SM_USRVDELM)	/* Users can't view deleted msgs */
					&& !is_user_subop(scfg, p->p->smb.subnum, user, client)) 	/* not sub-op */
					break;
				if(user==NULL)
					break;
				if(!is_user_subop(scfg, p->p->smb.subnum, user, client)			/* not sub-op */
					&& p->msg.idx.from!=namecrc && p->msg.idx.from!=aliascrc)	/* not for you */
					break; 
			}

			if((p->msg.idx.attr&MSG_MODERATED) && !(p->msg.idx.attr&MSG_VALIDATED)
				&& (!is_user_subop(scfg, p->p->smb.subnum, user, client)))
				break;

			if(p->msg.idx.attr&MSG_PRIVATE) {
				if(user==NULL)
					break;
				if(!is_user_subop(scfg, p->p->smb.subnum, user, client) && !(user->rest&FLAG('Q'))) {
					if(p->msg.idx.to!=namecrc && p->msg.idx.from!=namecrc
						&& p->msg.idx.to!=aliascrc && p->msg.idx.from!=aliascrc
						&& (user->number!=1 || p->msg.idx.to!=sysop))
						break;
					if(stricmp(p->msg.to,user->alias)
						&& stricmp(p->msg.from,user->alias)
						&& stricmp(p->msg.to,user->name)
						&& stricmp(p->msg.from,user->name)
						&& (user->number!=1 || stricmp(p->msg.to,"sysop")
						|| p->msg.from_net.type)) {
						break;
					}
				}
			}

			v=BOOLEAN_TO_JSVAL(JS_TRUE);
		} while(0);

		JS_DefineProperty(cx, obj, "can_read", v, NULL,NULL,JSPROP_ENUMERATE);

		if(name)
			return JS_TRUE;
	}
	if(name) free(name);

	/* DO NOT RETURN JS_FALSE on unknown names */
	/* Doing so will prevent toString() among others from working. */

	return JS_TRUE;
}

static JSBool js_get_msg_header_enumerate(JSContext *cx, JSObject *obj)
{
	privatemsg_t* p;

	js_get_msg_header_resolve(cx, obj, JSID_VOID);

	if((p=(privatemsg_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_TRUE;

	smb_freemsgmem(&(p->msg));
	free(p);

	JS_SetPrivate(cx, obj, NULL);

	return JS_TRUE;
}

static void js_get_msg_header_finalize(JSContext *cx, JSObject *obj)
{
	privatemsg_t* p;

	if((p=(privatemsg_t*)JS_GetPrivate(cx,obj))==NULL)
		return;

	smb_freemsgmem(&(p->msg));
	free(p);

	JS_SetPrivate(cx, obj, NULL);
}

static JSClass js_msghdr_class = {
     "MsgHeader"			/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,JS_PropertyStub		/* getProperty	*/
	,JS_StrictPropertyStub		/* setProperty	*/
	,js_get_msg_header_enumerate		/* enumerate	*/
	,js_get_msg_header_resolve			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_get_msg_header_finalize		/* finalize		*/
};

static JSBool
js_get_msg_header(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj=JS_THIS_OBJECT(cx, arglist);
	jsval*		argv=JS_ARGV(cx, arglist);
	uintN		n;
	JSObject*	hdrobj;
	JSBool		by_offset=JS_FALSE;
	JSBool		include_votes=JS_FALSE;
	jsrefcount	rc;
	char*		cstr = NULL;
	privatemsg_t*	p;
	JSObject*	proto;
	jsval		val;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if((p=(privatemsg_t*)malloc(sizeof(privatemsg_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return JS_FALSE;
	}

	memset(p,0,sizeof(privatemsg_t));

	if((p->p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		free(p);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(p->p->smb))) {
		free(p);
		return JS_TRUE;
	}

	p->expand_fields=JS_TRUE;	/* This parameter defaults to true */
	n=0;
	if(n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		by_offset = JSVAL_TO_BOOLEAN(argv[n++]);

	/* Now parse message offset/id and get message */
	if(n < argc && JSVAL_IS_NUMBER(argv[n])) {
		if(by_offset) {							/* Get by offset */
			if(!JS_ValueToInt32(cx,argv[n++],(int32*)&(p->msg).offset)) {
				free(p);
				return JS_FALSE;
			}
		}
		else {									/* Get by number */
			if(!JS_ValueToInt32(cx,argv[n++],(int32*)&(p->msg).hdr.number)) {
				free(p);
				return JS_FALSE;
			}
		}

		rc=JS_SUSPENDREQUEST(cx);
		if((p->p->smb_result=smb_getmsgidx(&(p->p->smb), &(p->msg)))!=SMB_SUCCESS) {
			JS_RESUMEREQUEST(cx, rc);
			free(p);
			return JS_TRUE;
		}

		if((p->p->smb_result=smb_lockmsghdr(&(p->p->smb),&(p->msg)))!=SMB_SUCCESS) {
			JS_RESUMEREQUEST(cx, rc);
			free(p);
			return JS_TRUE;
		}

		if((p->p->smb_result=smb_getmsghdr(&(p->p->smb), &(p->msg)))!=SMB_SUCCESS) {
			smb_unlockmsghdr(&(p->p->smb),&(p->msg)); 
			JS_RESUMEREQUEST(cx, rc);
			free(p);
			return JS_TRUE;
		}

		smb_unlockmsghdr(&(p->p->smb),&(p->msg)); 
		JS_RESUMEREQUEST(cx, rc);
	} else if(n < argc && JSVAL_IS_STRING(argv[n]))	{		/* Get by ID */
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[n]), cstr, NULL);
		n++;
		if(JS_IsExceptionPending(cx)) {
			free(cstr);
			free(p);
			return JS_FALSE;
		}
		if(cstr != NULL) {
			rc=JS_SUSPENDREQUEST(cx);
			if((p->p->smb_result=smb_getmsghdr_by_msgid(&(p->p->smb),&(p->msg)
					,cstr))!=SMB_SUCCESS) {
				free(cstr);
				JS_RESUMEREQUEST(cx, rc);
				free(p);
				return JS_TRUE;	/* ID not found */
			}
			free(cstr);
			JS_RESUMEREQUEST(cx, rc);
		}
	}

	if(p->msg.hdr.number==0) { /* No valid message number/id/offset specified */
		free(p);
		return JS_TRUE;
	}

	if(n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		p->expand_fields = JSVAL_TO_BOOLEAN(argv[n++]);

	if(n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		include_votes = JSVAL_TO_BOOLEAN(argv[n++]);

	if(!include_votes && (p->msg.hdr.attr&MSG_VOTE)) {
		smb_freemsgmem(&(p->msg));
		free(p);
		return JS_TRUE;
	}

	if(JS_GetProperty(cx, JS_GetGlobalObject(cx), "MsgBase", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx,val,&proto);
		if(JS_GetProperty(cx, proto, "HeaderPrototype", &val) && !JSVAL_NULL_OR_VOID(val))
			JS_ValueToObject(cx,val,&proto);
		else
			proto=NULL;
	}
	else
		proto=NULL;

	if((hdrobj=JS_NewObject(cx,&js_msghdr_class,proto,obj))==NULL) {
		smb_freemsgmem(&(p->msg));
		free(p);
		return JS_TRUE;
	}

	if(!JS_SetPrivate(cx, hdrobj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		free(p);
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(hdrobj));

	return JS_TRUE;
}

static JSBool
js_get_all_msg_headers(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj=JS_THIS_OBJECT(cx, arglist);
	jsval*		argv=JS_ARGV(cx, arglist);
	JSObject*	hdrobj;
	jsrefcount	rc;
	privatemsg_t*	p;
	private_t*	priv;
	JSObject*	proto;
	jsval		val;
	uint32_t	off;
    JSObject*	retobj;
	char		numstr[16];
	JSBool		include_votes=JS_FALSE;
	post_t*		post;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if((priv=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(priv->smb)))
		return JS_TRUE;

	if((post = malloc(priv->smb.status.total_msgs * sizeof(post_t))) == NULL) {
		JS_ReportError(cx, "malloc error", WHERE);
		return JS_FALSE;
	}
	memset(post, 0, priv->smb.status.total_msgs * sizeof(post_t));

	if(argc && JSVAL_IS_BOOLEAN(argv[0]))
		include_votes = JSVAL_TO_BOOLEAN(argv[0]);

    retobj = JS_NewObject(cx, NULL, NULL, obj);
    JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(retobj));

	rc=JS_SUSPENDREQUEST(cx);
	if((priv->smb_result=smb_locksmbhdr(&(priv->smb)))!=SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		free(post);
		return JS_TRUE;
	}
	JS_RESUMEREQUEST(cx, rc);

	if(JS_GetProperty(cx, JS_GetGlobalObject(cx), "MsgBase", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx,val,&proto);
		if(JS_GetProperty(cx, proto, "HeaderPrototype", &val) && !JSVAL_NULL_OR_VOID(val))
			JS_ValueToObject(cx,val,&proto);
		else
			proto=NULL;
	}
	else
		proto=NULL;

	for(off=0; off < priv->smb.status.total_msgs; off++) {
		smbmsg_t msg;

		ZERO_VAR(msg);
		msg.offset = off;

		rc=JS_SUSPENDREQUEST(cx);
		priv->smb_result = smb_getmsgidx(&(priv->smb), &msg);
		JS_RESUMEREQUEST(cx, rc);
		if(priv->smb_result != SMB_SUCCESS) {
			smb_unlocksmbhdr(&(priv->smb)); 
			free(post);
			return JS_TRUE;
		}
		post[off].idx = msg.idx;
		if(msg.idx.attr&MSG_VOTE && !(msg.idx.attr&MSG_POLL)) {
			ulong u;
			for(u = 0; u < off; u++)
				if(post[u].idx.number == msg.idx.remsg)
					break;
			if(u < off) {
				post[u].total_votes++;
				switch(msg.idx.attr&MSG_VOTE) {
				case MSG_UPVOTE:
					post[u].upvotes++;
					break;
				case MSG_DOWNVOTE:
					post[u].downvotes++;
					break;
				default:
					for(int b=0; b < MSG_POLL_MAX_ANSWERS; b++) {
						if(msg.idx.votes&(1<<b))
							post[u].votes[b]++;
					}
				}
			}
		}
	}

	for(off=0; off < priv->smb.status.total_msgs; off++) {
		if((!include_votes) && (post[off].idx.attr&MSG_VOTE))
			continue;

		if((p=(privatemsg_t*)malloc(sizeof(privatemsg_t)))==NULL) {
			smb_unlocksmbhdr(&(priv->smb)); 
			JS_ReportError(cx,"malloc failed");
			free(post);
			return JS_FALSE;
		}

		memset(p,0,sizeof(privatemsg_t));

		/* Parse boolean arguments first */
		p->p=priv;
		p->expand_fields=JS_TRUE;	/* This parameter defaults to true */

		p->msg.idx = post[off].idx;

		rc=JS_SUSPENDREQUEST(cx);
		priv->smb_result = smb_getmsghdr(&(priv->smb), &(p->msg));
		JS_RESUMEREQUEST(cx, rc);
		if(priv->smb_result != SMB_SUCCESS) {
			smb_unlocksmbhdr(&(priv->smb)); 
			free(post);
			free(p);
			return JS_TRUE;
		}

		if((hdrobj=JS_NewObject(cx,&js_msghdr_class,proto,obj))==NULL) {
			smb_freemsgmem(&(p->msg));
			smb_unlocksmbhdr(&(priv->smb)); 
			free(post);
			free(p);
			return JS_TRUE;
		}

		JS_DefineProperty(cx, hdrobj, "total_votes", UINT_TO_JSVAL(post[off].total_votes)
			,NULL, NULL, JSPROP_ENUMERATE);
		if(post[off].upvotes)
			JS_DefineProperty(cx, hdrobj, "upvotes", UINT_TO_JSVAL(post[off].upvotes)
				,NULL, NULL, JSPROP_ENUMERATE);
		if(post[off].downvotes)
			JS_DefineProperty(cx, hdrobj, "downvotes", UINT_TO_JSVAL(post[off].downvotes)
				,NULL, NULL, JSPROP_ENUMERATE);
		if(post[off].total_votes) {
			JSObject*		array;
			if((array=JS_NewArrayObject(cx,0,NULL)) != NULL) {
				JS_DefineProperty(cx, hdrobj, "tally", OBJECT_TO_JSVAL(array)
					,NULL, NULL, JSPROP_ENUMERATE);
				for(int i=0; i < MSG_POLL_MAX_ANSWERS;i++)
					JS_DefineElement(cx, array, i, UINT_TO_JSVAL(post[off].votes[i])
						,NULL, NULL, JSPROP_ENUMERATE);
			}
		}

		if(!JS_SetPrivate(cx, hdrobj, p)) {
			JS_ReportError(cx,"JS_SetPrivate failed");
			smb_unlocksmbhdr(&(priv->smb));
			free(post);
			free(p);
			return JS_FALSE;
		}

		val=OBJECT_TO_JSVAL(hdrobj);
		sprintf(numstr,"%"PRIu32, p->msg.hdr.number);
		JS_SetProperty(cx, retobj, numstr, &val);
	}
	smb_unlocksmbhdr(&(priv->smb));
	free(post);

	return JS_TRUE;
}

static JSBool
js_put_msg_header(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		n;
	JSBool		by_offset=JS_FALSE;
	JSBool		msg_specified=JS_FALSE;
	smbmsg_t	msg;
	JSObject*	hdr=NULL;
	private_t*	p;
	jsrefcount	rc;
	char*		cstr;
	JSBool		ret=JS_TRUE;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	memset(&msg,0,sizeof(msg));

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_BOOLEAN(argv[n]))
			by_offset=JSVAL_TO_BOOLEAN(argv[n]);
		else if(JSVAL_IS_NUMBER(argv[n])) {
			if(by_offset) {							/* Get by offset */
				if(!JS_ValueToInt32(cx,argv[n],(int32*)&msg.offset))
					return JS_FALSE;
			}
			else {									/* Get by number */
				if(!JS_ValueToInt32(cx,argv[n],(int32*)&msg.hdr.number))
					return JS_FALSE;
			}
			msg_specified=JS_TRUE;
			n++;
			break;
		} else if(JSVAL_IS_STRING(argv[n]))	{		/* Get by ID */
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[n]), cstr, NULL);
			HANDLE_PENDING(cx, cstr);
			rc=JS_SUSPENDREQUEST(cx);
			if(!msg_offset_by_id(p
					,cstr
					,&msg.offset)) {
				free(cstr);
				JS_RESUMEREQUEST(cx, rc);
				return JS_TRUE;	/* ID not found */
			}
			free(cstr);
			JS_RESUMEREQUEST(cx, rc);
			msg_specified=JS_TRUE;
			n++;
			break;
		}
	}

	if(!msg_specified)
		return JS_TRUE;

	if(n==argc || !JSVAL_IS_OBJECT(argv[n])) /* no header supplied? */
		return JS_TRUE;

	hdr = JSVAL_TO_OBJECT(argv[n++]);

	privatemsg_t* mp;
	mp=(privatemsg_t*)JS_GetPrivate(cx,hdr);
	if(mp != NULL && mp->expand_fields) {
		JS_ReportError(cx, "Message header has 'expanded fields'", WHERE);
		return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	if((p->smb_result=smb_getmsgidx(&(p->smb), &msg))!=SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}

	if((p->smb_result=smb_lockmsghdr(&(p->smb),&msg))!=SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}

	do {
		if((p->smb_result=smb_getmsghdr(&(p->smb), &msg))!=SMB_SUCCESS)
			break;

		smb_freemsghdrmem(&msg);	/* prevent duplicate header fields */

		JS_RESUMEREQUEST(cx, rc);
		if(!parse_header_object(cx, p, hdr, &msg, TRUE)) {
			SAFECOPY(p->smb.last_error,"Header parsing failure (required field missing?)");
			ret=JS_FALSE;
			break;
		}
		rc=JS_SUSPENDREQUEST(cx);

		if((p->smb_result=smb_putmsg(&(p->smb), &msg))!=SMB_SUCCESS)
			break;

		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} while(0);

	smb_unlockmsghdr(&(p->smb),&msg); 
	smb_freemsgmem(&msg);
	JS_RESUMEREQUEST(cx, rc);

	return(ret);
}

static JSBool
js_remove_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		n;
	JSBool		by_offset=JS_FALSE;
	JSBool		msg_specified=JS_FALSE;
	smbmsg_t	msg;
	private_t*	p;
	char*		cstr = NULL;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	memset(&msg,0,sizeof(msg));

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_BOOLEAN(argv[n]))
			by_offset=JSVAL_TO_BOOLEAN(argv[n]);
		else if(JSVAL_IS_NUMBER(argv[n])) {
			if(by_offset) {							/* Get by offset */
				if(!JS_ValueToInt32(cx,argv[n],(int32*)&msg.offset))
					return JS_FALSE;
			}
			else {									/* Get by number */
				if(!JS_ValueToInt32(cx,argv[n],(int32*)&msg.hdr.number))
					return JS_FALSE;
			}
			msg_specified=JS_TRUE;
			n++;
			break;
		} else if(JSVAL_IS_STRING(argv[n]))	{		/* Get by ID */
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[n]), cstr, NULL);
			HANDLE_PENDING(cx, cstr);
			if(cstr == NULL)
				return JS_FALSE;
			rc=JS_SUSPENDREQUEST(cx);
			if(!msg_offset_by_id(p
					,cstr
					,&msg.offset)) {
				free(cstr);
				JS_RESUMEREQUEST(cx, rc);
				return JS_TRUE;	/* ID not found */
			}
			free(cstr);
			JS_RESUMEREQUEST(cx, rc);
			msg_specified=JS_TRUE;
			n++;
			break;
		}
	}

	if(!msg_specified)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	if((p->smb_result=smb_getmsgidx(&(p->smb), &msg))==SMB_SUCCESS
		&& (p->smb_result=smb_getmsghdr(&(p->smb), &msg))==SMB_SUCCESS) {

		msg.hdr.attr|=MSG_DELETE;

		if((p->smb_result=smb_updatemsg(&(p->smb), &msg))==SMB_SUCCESS)
			JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	}

	smb_freemsgmem(&msg);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static char* get_msg_text(private_t* p, smbmsg_t* msg, BOOL strip_ctrl_a, BOOL rfc822, ulong mode, JSBool existing)
{
	char*		buf;

	if(existing) {
		if((p->smb_result=smb_lockmsghdr(&(p->smb),msg))!=SMB_SUCCESS)
			return(NULL);
	}
	else {
		if((p->smb_result=smb_getmsgidx(&(p->smb), msg))!=SMB_SUCCESS)
			return(NULL);

		if((p->smb_result=smb_lockmsghdr(&(p->smb),msg))!=SMB_SUCCESS)
			return(NULL);

		if((p->smb_result=smb_getmsghdr(&(p->smb), msg))!=SMB_SUCCESS) {
			smb_unlockmsghdr(&(p->smb), msg); 
			return(NULL);
		}
	}

	if((buf=smb_getmsgtxt(&(p->smb), msg, mode))==NULL) {
		smb_unlockmsghdr(&(p->smb),msg); 
		if(!existing)
			smb_freemsgmem(msg);
		return(NULL);
	}

	smb_unlockmsghdr(&(p->smb), msg); 
	if(!existing)
		smb_freemsgmem(msg);

	if(strip_ctrl_a)
		remove_ctrl_a(buf, buf);

	if(rfc822) {	/* must escape lines starting with dot ('.') */
		char* newbuf;
		if((newbuf=malloc((strlen(buf)*2)+1))!=NULL) {
			int i,j;
			for(i=j=0;buf[i];i++) {
				if((i==0 || buf[i-1]=='\n') && buf[i]=='.')
					newbuf[j++]='.';
				newbuf[j++]=buf[i]; 
			}
			newbuf[j]=0;
			free(buf);
			buf = newbuf;
		}
	}

	return(buf);
}

static JSBool
js_get_msg_body(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		buf;
	uintN		n;
	smbmsg_t	msg;
	smbmsg_t	*msgptr;
	long		getmsgtxtmode = 0;
	JSBool		by_offset=JS_FALSE;
	JSBool		strip_ctrl_a=JS_FALSE;
	JSBool		tails=JS_TRUE;
	JSBool		plain=JS_FALSE;
	JSBool		rfc822=JS_FALSE;
	JSBool		msg_specified=JS_FALSE;
	JSBool		existing_msg=JS_FALSE;
	JSString*	js_str;
	private_t*	p;
	char*		cstr;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	memset(&msg,0,sizeof(msg));
	msgptr=&msg;

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_BOOLEAN(argv[n]))
			by_offset=JSVAL_TO_BOOLEAN(argv[n]);
		else if(JSVAL_IS_NUMBER(argv[n])) {
			if(by_offset) {							/* Get by offset */
				if(!JS_ValueToInt32(cx,argv[n],(int32*)&msg.offset))
					return JS_FALSE;
			}
			else {									/* Get by number */
				if(!JS_ValueToInt32(cx,argv[n],(int32*)&msg.hdr.number))
					return JS_FALSE;
			}
			msg_specified=JS_TRUE;
			n++;
			break;
		} else if(JSVAL_IS_STRING(argv[n]))	{		/* Get by ID */
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[n]), cstr, NULL);
			HANDLE_PENDING(cx, cstr);
			rc=JS_SUSPENDREQUEST(cx);
			if(!msg_offset_by_id(p
					,cstr
					,&msg.offset)) {
				free(cstr);
				JS_RESUMEREQUEST(cx, rc);
				return JS_TRUE;	/* ID not found */
			}
			free(cstr);
			JS_RESUMEREQUEST(cx, rc);
			msg_specified=JS_TRUE;
			n++;
			break;
		} else if(JSVAL_IS_OBJECT(argv[n])) {		/* Use existing header */
			JSClass *oc=JS_GetClass(cx, JSVAL_TO_OBJECT(argv[n]));
			if(strcmp(oc->name, js_msghdr_class.name)==0) {
				privatemsg_t	*pmsg=JS_GetPrivate(cx,JSVAL_TO_OBJECT(argv[n]));

				if(pmsg != NULL) {
					msg_specified=JS_TRUE;
					existing_msg=JS_TRUE;
					msgptr=&pmsg->msg;
				}
			}
			n++;
			break;
		}
	
	}

	if(!msg_specified)	/* No message number or id specified */
		return JS_TRUE;

	if(n<argc && JSVAL_IS_BOOLEAN(argv[n]))
		strip_ctrl_a=JSVAL_TO_BOOLEAN(argv[n++]);

	if(n<argc && JSVAL_IS_BOOLEAN(argv[n]))
		rfc822=JSVAL_TO_BOOLEAN(argv[n++]);

	if(n<argc && JSVAL_IS_BOOLEAN(argv[n]))
		tails=JSVAL_TO_BOOLEAN(argv[n++]);

	if(n<argc && JSVAL_IS_BOOLEAN(argv[n]))
		plain=JSVAL_TO_BOOLEAN(argv[n++]);

	if(tails)
		getmsgtxtmode |= GETMSGTXT_TAILS;

	if(plain)
		getmsgtxtmode |= GETMSGTXT_PLAIN;

	rc=JS_SUSPENDREQUEST(cx);
	buf = get_msg_text(p, msgptr, strip_ctrl_a, rfc822, getmsgtxtmode, existing_msg);
	JS_RESUMEREQUEST(cx, rc);
	if(buf==NULL)
		return JS_TRUE;

	if((js_str=JS_NewStringCopyZ(cx,buf))!=NULL)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));

	smb_freemsgtxt(buf);

	return JS_TRUE;
}

static JSBool
js_get_msg_tail(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		buf;
	uintN		n;
	smbmsg_t	msg;
	smbmsg_t	*msgptr;
	JSBool		by_offset=JS_FALSE;
	JSBool		strip_ctrl_a=JS_FALSE;
	JSBool		rfc822=JS_FALSE;
	JSBool		msg_specified=JS_FALSE;
	JSBool		existing_msg=JS_FALSE;
	JSString*	js_str;
	private_t*	p;
	char*		cstr;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	memset(&msg,0,sizeof(msg));
	msgptr=&msg;

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_BOOLEAN(argv[n]))
			by_offset=JSVAL_TO_BOOLEAN(argv[n]);
		else if(JSVAL_IS_NUMBER(argv[n])) {
			if(by_offset) {							/* Get by offset */
				if(!JS_ValueToInt32(cx,argv[n],(int32*)&msg.offset))
					return JS_FALSE;
			}
			else {									/* Get by number */
				if(!JS_ValueToInt32(cx,argv[n],(int32*)&msg.hdr.number))
					return JS_FALSE;
			}
			msg_specified=JS_TRUE;
			n++;
			break;
		} else if(JSVAL_IS_STRING(argv[n]))	{		/* Get by ID */
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[n]), cstr, NULL);
			HANDLE_PENDING(cx, cstr);
			rc=JS_SUSPENDREQUEST(cx);
			if(!msg_offset_by_id(p
					,cstr
					,&msg.offset)) {
				free(cstr);
				JS_RESUMEREQUEST(cx, rc);
				return JS_TRUE;	/* ID not found */
			}
			free(cstr);
			JS_RESUMEREQUEST(cx, rc);
			msg_specified=JS_TRUE;
			n++;
			break;
		} else if(JSVAL_IS_OBJECT(argv[n])) {		/* Use existing header */
			JSClass *oc=JS_GetClass(cx, JSVAL_TO_OBJECT(argv[n]));
			if(strcmp(oc->name, js_msghdr_class.name)==0) {
				privatemsg_t	*pmsg=JS_GetPrivate(cx,JSVAL_TO_OBJECT(argv[n]));

				if(pmsg != NULL) {
					msg_specified=JS_TRUE;
					existing_msg=JS_TRUE;
					msgptr=&pmsg->msg;
				}
			}
			n++;
			break;
		}
	}

	if(!msg_specified)	/* No message number or id specified */
		return JS_TRUE;

	if(n<argc && JSVAL_IS_BOOLEAN(argv[n]))
		strip_ctrl_a=JSVAL_TO_BOOLEAN(argv[n++]);

	if(n<argc && JSVAL_IS_BOOLEAN(argv[n]))
		rfc822=JSVAL_TO_BOOLEAN(argv[n++]);

	rc=JS_SUSPENDREQUEST(cx);
	buf = get_msg_text(p, msgptr, strip_ctrl_a, rfc822, GETMSGTXT_TAILS|GETMSGTXT_NO_BODY, existing_msg);
	JS_RESUMEREQUEST(cx, rc);
	if(buf==NULL)
		return JS_TRUE;

	if((js_str=JS_NewStringCopyZ(cx,buf))!=NULL)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));

	smb_freemsgtxt(buf);

	return JS_TRUE;
}

static JSBool
js_save_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		body=NULL;
	uintN		n;
	jsuint      i;
	jsuint      rcpt_list_length=0;
	jsval       val;
	JSObject*	hdr=NULL;
	JSObject*	objarg;
	JSObject*	rcpt_list=NULL;
	JSClass*	cl;
	smbmsg_t	rcpt_msg;
	smbmsg_t	msg;
	client_t*	client=NULL;
	private_t*	p;
	JSBool		ret=JS_TRUE;
	jsrefcount	rc;
	scfg_t*			scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if(argc<2)
		return JS_TRUE;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(p->smb))) {
		if(!js_open(cx, 0, arglist))
			return JS_FALSE;
		if(JS_RVAL(cx, arglist) == JSVAL_FALSE)
			return JS_TRUE;
	}

	memset(&msg,0,sizeof(msg));

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			objarg = JSVAL_TO_OBJECT(argv[n]);
			if((cl=JS_GetClass(cx,objarg))!=NULL && strcmp(cl->name,"Client")==0) {
				client=JS_GetPrivate(cx,objarg);
				continue;
			}
			if(JS_IsArrayObject(cx, objarg)) {		/* recipient_list is an array of objects */
				if(body!=NULL && rcpt_list==NULL) {	/* body text already specified */
					rcpt_list = objarg;
					continue;
				}
			}
			else if(hdr==NULL) {
				hdr = objarg;
				continue;
			}
		}
		if(body==NULL) {
			JSVALUE_TO_MSTRING(cx, argv[n], body, NULL);
			HANDLE_PENDING(cx, body);
			if(body==NULL) {
				JS_ReportError(cx,"Invalid message body string");
				return JS_FALSE;
			}
		}
	}

	// Find and use the global client object if possible...
	if(client==NULL) {
		if(JS_GetProperty(cx, JS_GetGlobalObject(cx), "client", &val) && !JSVAL_NULL_OR_VOID(val)) {
			objarg = JSVAL_TO_OBJECT(val);
			if((cl=JS_GetClass(cx,objarg))!=NULL && strcmp(cl->name,"Client")==0)
				client=JS_GetPrivate(cx,objarg);
		}
	}

	if(hdr==NULL) {
		FREE_AND_NULL(body);
		return JS_TRUE;
	}
	if(body==NULL)
		body=strdup("");

	if(rcpt_list!=NULL) {
		if(!JS_GetArrayLength(cx, rcpt_list, &rcpt_list_length) || !rcpt_list_length) {
			free(body);
			return JS_TRUE;
		}
	}

	if(parse_header_object(cx, p, hdr, &msg, rcpt_list==NULL)) {

		if(body[0])
			truncsp(body);
		rc=JS_SUSPENDREQUEST(cx);
		if((p->smb_result=savemsg(scfg, &(p->smb), &msg, client, /* ToDo server hostname: */NULL, body))==SMB_SUCCESS) {
			JS_RESUMEREQUEST(cx, rc);
			JS_SET_RVAL(cx, arglist, JSVAL_TRUE);

			if(rcpt_list!=NULL) {	/* Sending to a list of recipients */

				JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
				SAFECOPY(p->smb.last_error,"Recipient list parsing failure");

				memset(&rcpt_msg, 0, sizeof(rcpt_msg));

				for(i=0;i<rcpt_list_length;i++) {

					if(!JS_GetElement(cx, rcpt_list, i, &val))
						break;

					if(!JSVAL_IS_OBJECT(val))
						break;

					rc=JS_SUSPENDREQUEST(cx);
					if((p->smb_result=smb_copymsgmem(&(p->smb), &rcpt_msg, &msg))!=SMB_SUCCESS) {
						JS_RESUMEREQUEST(cx, rc);
						break;
					}
					JS_RESUMEREQUEST(cx, rc);

					if(!parse_recipient_object(cx, p, JSVAL_TO_OBJECT(val), &rcpt_msg))
						break;

					rc=JS_SUSPENDREQUEST(cx);
					if((p->smb_result=smb_addmsghdr(&(p->smb), &rcpt_msg, smb_storage_mode(scfg, &(p->smb))))!=SMB_SUCCESS) {
						JS_RESUMEREQUEST(cx, rc);
						break;
					}

					smb_freemsgmem(&rcpt_msg);
					JS_RESUMEREQUEST(cx, rc);
				}
				smb_freemsgmem(&rcpt_msg);	/* just in case we broke the loop */

				if(i==rcpt_list_length)
					JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
				if(i > 1)
					smb_incmsg_dfields(&(p->smb), &msg, (uint16_t)(i - 1));
			}
		}
		else
			JS_RESUMEREQUEST(cx, rc);
	} else {
		ret=JS_FALSE;
		SAFECOPY(p->smb.last_error,"Header parsing failure (required field missing?)");
	}
	free(body);

	smb_freemsgmem(&msg);

	return(ret);
}

static JSBool
js_vote_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj=JS_THIS_OBJECT(cx, arglist);
	jsval*		argv=JS_ARGV(cx, arglist);
	uintN		n;
	JSObject*	hdr=NULL;
	JSObject*	objarg;
	smbmsg_t	msg;
	private_t*	p;
	JSBool		ret=JS_TRUE;
	jsrefcount	rc;
	scfg_t*		scfg;

	scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if(argc < 1)
		return JS_TRUE;
	
	if((p=(private_t*)JS_GetPrivate(cx, obj)) == NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(p->smb))) {
		if(!js_open(cx, 0, arglist))
			return JS_FALSE;
		if(JS_RVAL(cx, arglist) == JSVAL_FALSE)
			return JS_TRUE;
	}

	memset(&msg, 0, sizeof(msg));
	msg.hdr.type = SMB_MSG_TYPE_BALLOT;

	for(n=0; n<argc; n++) {
		if(JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			objarg = JSVAL_TO_OBJECT(argv[n]);
			if(hdr == NULL) {
				hdr = objarg;
				continue;
			}
		}
	}

	if(hdr == NULL)
		return JS_TRUE;

	if(parse_header_object(cx, p, hdr, &msg, FALSE)) {

		rc = JS_SUSPENDREQUEST(cx);
		if((p->smb_result=votemsg(scfg, &(p->smb), &msg, NULL, NULL)) == SMB_SUCCESS) {
			JS_RESUMEREQUEST(cx, rc);
			JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		}
		else
			JS_RESUMEREQUEST(cx, rc);
	} else {
		ret = JS_FALSE;
		SAFECOPY(p->smb.last_error,"Header parsing failure (required field missing?)");
	}

	smb_freemsgmem(&msg);

	return ret;
}

static JSBool
js_add_poll(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj=JS_THIS_OBJECT(cx, arglist);
	jsval*		argv=JS_ARGV(cx, arglist);
	uintN		n;
	JSObject*	hdr=NULL;
	JSObject*	objarg;
	smbmsg_t	msg;
	private_t*	p;
	JSBool		ret=JS_TRUE;
	jsrefcount	rc;
	scfg_t*		scfg;

	scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if(argc < 1)
		return JS_TRUE;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj)) == NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(p->smb))) {
		if(!js_open(cx, 0, arglist))
			return JS_FALSE;
		if(JS_RVAL(cx, arglist) == JSVAL_FALSE)
			return JS_TRUE;
	}

	memset(&msg, 0, sizeof(msg));
	msg.hdr.type = SMB_MSG_TYPE_POLL;

	for(n=0; n<argc; n++) {
		if(JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			objarg = JSVAL_TO_OBJECT(argv[n]);
			if(hdr == NULL) {
				hdr = objarg;
				continue;
			}
		}
	}

	if(hdr == NULL)
		return JS_TRUE;

	if(parse_header_object(cx, p, hdr, &msg, FALSE)) {

		rc=JS_SUSPENDREQUEST(cx);
		if((p->smb_result=postpoll(scfg, &(p->smb), &msg)) == SMB_SUCCESS) {
			JS_RESUMEREQUEST(cx, rc);
			JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		}
		else
			JS_RESUMEREQUEST(cx, rc);
	} else {
		ret = JS_FALSE;
		SAFECOPY(p->smb.last_error,"Header parsing failure (required field missing?)");
	}

	smb_freemsgmem(&msg);

	return ret;
}

static JSBool
js_how_user_voted(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj=JS_THIS_OBJECT(cx, arglist);
	jsval*		argv=JS_ARGV(cx, arglist);
	int32		msgnum;
	private_t*	p;
	char*		name = NULL;
	uint16_t	votes;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	if(!JS_ValueToInt32(cx, argv[0], &msgnum))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[1], name, NULL)
	HANDLE_PENDING(cx, name);
	if(name==NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	votes = smb_voted_already(&(p->smb), msgnum, name, NET_NONE, NULL);
	free(name);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist,UINT_TO_JSVAL(votes));

	return JS_TRUE;
}

static JSBool
js_close_poll(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj=JS_THIS_OBJECT(cx, arglist);
	jsval*		argv=JS_ARGV(cx, arglist);
	int32		msgnum;
	private_t*	p;
	char*		name = NULL;
	int			result;
	jsrefcount	rc;
	scfg_t*		scfg;

	scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	if(!JS_ValueToInt32(cx, argv[0], &msgnum))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[1], name, NULL)
	HANDLE_PENDING(cx, name);
	if(name==NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	result = closepoll(scfg, &(p->smb), msgnum, name);
	free(name);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, result == SMB_SUCCESS ? JSVAL_TRUE : JSVAL_FALSE);

	return JS_TRUE;
}

/* MsgBase Object Properties */
enum {
	 SMB_PROP_LAST_ERROR
	,SMB_PROP_FILE		
	,SMB_PROP_DEBUG		
	,SMB_PROP_RETRY_TIME
	,SMB_PROP_RETRY_DELAY
	,SMB_PROP_FIRST_MSG		/* first message number */
	,SMB_PROP_LAST_MSG		/* last message number */
	,SMB_PROP_TOTAL_MSGS 	/* total messages */
	,SMB_PROP_MAX_CRCS		/* Maximum number of CRCs to keep in history */
    ,SMB_PROP_MAX_MSGS      /* Maximum number of message to keep in sub */
    ,SMB_PROP_MAX_AGE       /* Maximum age of message to keep in sub (in days) */
	,SMB_PROP_ATTR			/* Attributes for this message base (SMB_HYPER,etc) */
	,SMB_PROP_SUBNUM		/* sub-board number */
	,SMB_PROP_IS_OPEN
	,SMB_PROP_STATUS		/* Last SMBLIB returned status value (e.g. retval) */
};

static JSBool js_msgbase_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
    jsint       tiny;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return JS_FALSE;
	}

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case SMB_PROP_RETRY_TIME:
			if(!JS_ValueToInt32(cx,*vp,(int32*)&(p->smb).retry_time))
				return JS_FALSE;
			break;
		case SMB_PROP_RETRY_DELAY:
			if(!JS_ValueToInt32(cx,*vp,(int32*)&(p->smb).retry_delay))
				return JS_FALSE;
			break;
		case SMB_PROP_DEBUG:
			JS_ValueToBoolean(cx,*vp,&p->debug);
			break;
	}

	return JS_TRUE;
}

static JSBool js_msgbase_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
	char*		s=NULL;
	JSString*	js_str;
    jsint       tiny;
	idxrec_t	idx;
	private_t*	p;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case SMB_PROP_FILE:
			s=p->smb.file;
			break;
		case SMB_PROP_LAST_ERROR:
			s=p->smb.last_error;
			break;
		case SMB_PROP_STATUS:
			*vp = INT_TO_JSVAL(p->smb_result);
			break;
		case SMB_PROP_RETRY_TIME:
			*vp = INT_TO_JSVAL(p->smb.retry_time);
			break;
		case SMB_PROP_RETRY_DELAY:
			*vp = INT_TO_JSVAL(p->smb.retry_delay);
			break;
		case SMB_PROP_DEBUG:
			*vp = INT_TO_JSVAL(p->debug);
			break;
		case SMB_PROP_FIRST_MSG:
			rc=JS_SUSPENDREQUEST(cx);
			memset(&idx,0,sizeof(idx));
			smb_getfirstidx(&(p->smb),&idx);
			JS_RESUMEREQUEST(cx, rc);
			*vp=UINT_TO_JSVAL(idx.number);
			break;
		case SMB_PROP_LAST_MSG:
			rc=JS_SUSPENDREQUEST(cx);
			smb_getstatus(&(p->smb));
			JS_RESUMEREQUEST(cx, rc);
			*vp=UINT_TO_JSVAL(p->smb.status.last_msg);
			break;
		case SMB_PROP_TOTAL_MSGS:
			rc=JS_SUSPENDREQUEST(cx);
			smb_getstatus(&(p->smb));
			JS_RESUMEREQUEST(cx, rc);
			*vp=UINT_TO_JSVAL(p->smb.status.total_msgs);
			break;
		case SMB_PROP_MAX_CRCS:
			*vp=UINT_TO_JSVAL(p->smb.status.max_crcs);
			break;
		case SMB_PROP_MAX_MSGS:
			*vp=UINT_TO_JSVAL(p->smb.status.max_msgs);
			break;
		case SMB_PROP_MAX_AGE:
			*vp=UINT_TO_JSVAL(p->smb.status.max_age);
			break;
		case SMB_PROP_ATTR:
			*vp=UINT_TO_JSVAL(p->smb.status.attr);
			break;
		case SMB_PROP_SUBNUM:
			*vp = INT_TO_JSVAL(p->smb.subnum);
			break;
		case SMB_PROP_IS_OPEN:
			*vp = BOOLEAN_TO_JSVAL(SMB_IS_OPEN(&(p->smb)));
			break;
	}

	if(s!=NULL) {
		if((js_str=JS_NewStringCopyZ(cx, s))==NULL)
			return JS_FALSE;
		*vp = STRING_TO_JSVAL(js_str);
	}

	return JS_TRUE;
}

#define SMB_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_msgbase_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/

	{	"error"				,SMB_PROP_LAST_ERROR	,SMB_PROP_FLAGS,	310 },
	{	"last_error"		,SMB_PROP_LAST_ERROR	,JSPROP_READONLY,	311 },	/* alias */
	{	"status"			,SMB_PROP_STATUS		,SMB_PROP_FLAGS,	312 },
	{	"file"				,SMB_PROP_FILE			,SMB_PROP_FLAGS,	310 },
	{	"debug"				,SMB_PROP_DEBUG			,0,					310 },
	{	"retry_time"		,SMB_PROP_RETRY_TIME	,JSPROP_ENUMERATE,	310 },
	{	"retry_delay"		,SMB_PROP_RETRY_DELAY	,JSPROP_ENUMERATE,	311 },
	{	"first_msg"			,SMB_PROP_FIRST_MSG		,SMB_PROP_FLAGS,	310 },
	{	"last_msg"			,SMB_PROP_LAST_MSG		,SMB_PROP_FLAGS,	310 },
	{	"total_msgs"		,SMB_PROP_TOTAL_MSGS	,SMB_PROP_FLAGS,	310 },
	{	"max_crcs"			,SMB_PROP_MAX_CRCS		,SMB_PROP_FLAGS,	310 },
	{	"max_msgs"			,SMB_PROP_MAX_MSGS  	,SMB_PROP_FLAGS,	310 },
	{	"max_age"			,SMB_PROP_MAX_AGE   	,SMB_PROP_FLAGS,	310 },
	{	"attributes"		,SMB_PROP_ATTR			,SMB_PROP_FLAGS,	310 },
	{	"subnum"			,SMB_PROP_SUBNUM		,SMB_PROP_FLAGS,	310 },
	{	"is_open"			,SMB_PROP_IS_OPEN		,SMB_PROP_FLAGS,	310 },
	{0}
};

#ifdef BUILD_JSDOCS
static char* msgbase_prop_desc[] = {

	 "last occurred message base error - <small>READ ONLY</small>"
	,"return value of last <i>SMB Library</i> function call - <small>READ ONLY</small>"
	,"base path and filename of message base - <small>READ ONLY</small>"
	,"message base open/lock retry timeout (in seconds)"
	,"delay between message base open/lock retries (in milliseconds)"
	,"first message number - <small>READ ONLY</small>"
	,"last message number - <small>READ ONLY</small>"
	,"total number of messages - <small>READ ONLY</small>"
	,"maximum number of message CRCs to store (for dupe checking) - <small>READ ONLY</small>"
	,"maximum number of messages before expiration - <small>READ ONLY</small>"
	,"maximum age (in days) of messages to store - <small>READ ONLY</small>"
	,"message base attributes - <small>READ ONLY</small>"
	,"sub-board number (0-based, 65535 for e-mail) - <small>READ ONLY</small>"
	,"<i>true</i> if the message base has been opened successfully - <small>READ ONLY</small>"
	,NULL
};
#endif

static jsSyncMethodSpec js_msgbase_functions[] = {
	{"open",			js_open,			0, JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("open message base")
	,310
	},
	{"close",			js_close,			0, JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("close message base (if open)")
	,310
	},
	{"get_msg_header",	js_get_msg_header,	4, JSTYPE_OBJECT,	JSDOCSTR("[by_offset=<tt>false</tt>,] number_or_id [,expand_fields=<tt>true</tt>] [,include_votes=<tt>false</tt>]")
	,JSDOCSTR("returns a specific message header, <i>null</i> on failure. "
	"<br><i>New in v3.12:</i> Pass <i>false</i> for the <i>expand_fields</i> argument (default: <i>true</i>) "
	"if you will be re-writing the header later with <i>put_msg_header()</i>")
	,312
	},
	{"get_all_msg_headers", js_get_all_msg_headers, 1, JSTYPE_ARRAY, JSDOCSTR("[include_votes=<tt>false</tt>]")
	,JSDOCSTR("returns an object of all message headers indexed by message number.<br>"
	"Message headers returned by this function include 2 additional properties: <tt>upvotes</tt> and <tt>downvotes</tt>.<br>"
	"Vote messages are excluded by default.")
	,316
	},
	{"put_msg_header",	js_put_msg_header,	2, JSTYPE_BOOLEAN,	JSDOCSTR("[by_offset=<tt>false</tt>,] number, object header")
	,JSDOCSTR("modify an existing message header")
	,310
	},
	{"get_msg_body",	js_get_msg_body,	2, JSTYPE_STRING,	JSDOCSTR("[by_offset=<tt>false</tt>,] number_or_id [, message_header] [,strip_ctrl_a=<tt>false</tt>] "
		"[,rfc822_encoded=<tt>false</tt>] [,include_tails=<tt>true</tt>] [,plain_text=<tt>false</tt>]")
	,JSDOCSTR("returns the entire body text of a specific message as a single String, <i>null</i> on failure. "
		"The default behavior is to leave Ctrl-A codes intact, perform no RFC-822 encoding, and to include tails (if any) in the "
		"returned body text. When <i>plain_text</i> is true, only the first plain-text portion of a multi-part MIME encoded message body is returned."
	)
	,310
	},
	{"get_msg_tail",	js_get_msg_tail,	2, JSTYPE_STRING,	JSDOCSTR("[by_offset=<tt>false</tt>,] number_or_id [, message_header] [,strip_ctrl_a]=<tt>false</tt>")
	,JSDOCSTR("returns the tail text of a specific message, <i>null</i> on failure")
	,310
	},
	{"get_msg_index",	js_get_msg_index,	3, JSTYPE_OBJECT,	JSDOCSTR("[by_offset=<tt>false</tt>,] number, [include_votes=<tt>false</tt>]")
	,JSDOCSTR("returns a specific message index, <i>null</i> on failure. "
	"The index object will contain the following properties:<br>"
	"<table>"
	"<tr><td align=top><tt>attr</tt><td>Attribute bitfield"
	"<tr><td align=top><tt>time</tt><td>Date/time imported (in time_t format)"
	"<tr><td align=top><tt>number</tt><td>Message number"
	"<tr><td align=top><tt>offset</tt><td>Record number in index file"
	"</table>"
	"Indexes of regular messages will contain the following additional properties:<br>"
	"<table>"
	"<tr><td align=top><tt>subject</tt><td>CRC-16 of lowercase message subject"
	"<tr><td align=top><tt>to</tt><td>CRC-16 of lowercase recipient's name (or user number if e-mail)"
	"<tr><td align=top><tt>from</tt><td>CRC-16 of lowercase sender's name (or user number if e-mail)"
	"</table>"
	"Indexes of vote messages will contain the following additional properties:<br>"
	"<table>"
	"<tr><td align=top><tt>vote</tt><td>vote value"
	"<tr><td align=top><tt>remsg</tt><td>number of message this vote is in response to"
	"</table>"
	)
	,311
	},
	{"remove_msg",		js_remove_msg,		2, JSTYPE_BOOLEAN,	JSDOCSTR("[by_offset=<tt>false</tt>,] number_or_id")
	,JSDOCSTR("mark message for deletion")
	,311
	},
	{"save_msg",		js_save_msg,		2, JSTYPE_BOOLEAN,	JSDOCSTR("object header [,client=<i>none</i>] [,body_text=<tt>\"\"</tt>] [,array rcpt_list=<i>none</i>]")
	,JSDOCSTR("create a new message in message base, the <i>header</i> object may contain the following properties:<br>"
	"<table>"
	"<tr><td align=top><tt>subject</tt><td>Message subject <i>(required)</i>"
	"<tr><td align=top><tt>to</tt><td>Recipient's name <i>(required)</i>"
	"<tr><td align=top><tt>to_ext</tt><td>Recipient's user number (for local e-mail)"
	"<tr><td align=top><tt>to_org</tt><td>Recipient's organization"
	"<tr><td align=top><tt>to_net_type</tt><td>Recipient's network type (default: 0 for local)"
	"<tr><td align=top><tt>to_net_addr</tt><td>Recipient's network address"
	"<tr><td align=top><tt>to_agent</tt><td>Recipient's agent type"
	"<tr><td align=top><tt>from</tt><td>Sender's name <i>(required)</i>"
	"<tr><td align=top><tt>from_ext</tt><td>Sender's user number"
	"<tr><td align=top><tt>from_org</tt><td>Sender's organization"
	"<tr><td align=top><tt>from_net_type</tt><td>Sender's network type (default: 0 for local)"
	"<tr><td align=top><tt>from_net_addr</tt><td>Sender's network address"
	"<tr><td align=top><tt>from_agent</tt><td>Sender's agent type"
	"<tr><td align=top><tt>from_ip_addr</tt><td>Sender's IP address (if available, for security tracking)"
	"<tr><td align=top><tt>from_host_name</tt><td>Sender's host name (if available, for security tracking)"
	"<tr><td align=top><tt>from_protocol</tt><td>TCP/IP protocol used by sender (if available, for security tracking)"
	"<tr><td align=top><tt>from_port</tt><td>TCP/UDP port number used by sender (if available, for security tracking)"
	"<tr><td align=top><tt>sender_userid</tt><td>Sender's user ID (if available, for security tracking)"
	"<tr><td align=top><tt>sender_server</tt><td>Server's host name (if available, for security tracking)"
	"<tr><td align=top><tt>sender_time</tt><td>Time/Date message was received from sender (if available, for security tracking)"
	"<tr><td align=top><tt>replyto</tt><td>Replies should be sent to this name"
	"<tr><td align=top><tt>replyto_ext</tt><td>Replies should be sent to this user number"
	"<tr><td align=top><tt>replyto_org</tt><td>Replies should be sent to organization"
	"<tr><td align=top><tt>replyto_net_type</tt><td>Replies should be sent to this network type"
	"<tr><td align=top><tt>replyto_net_addr</tt><td>Replies should be sent to this network address"
	"<tr><td align=top><tt>replyto_agent</tt><td>Replies should be sent to this agent type"
	"<tr><td align=top><tt>id</tt><td>Message's RFC-822 compliant Message-ID"
	"<tr><td align=top><tt>reply_id</tt><td>Message's RFC-822 compliant Reply-ID"
	"<tr><td align=top><tt>reverse_path</tt><td>Message's SMTP sender address"
	"<tr><td align=top><tt>forward_path</tt><td>Argument to SMTP 'RCPT TO' command"
	"<tr><td align=top><tt>path</tt><td>Messages's NNTP path"
	"<tr><td align=top><tt>newsgroups</tt><td>Message's NNTP newsgroups header"
	"<tr><td align=top><tt>ftn_msgid</tt><td>FidoNet FTS-9 Message-ID"
	"<tr><td align=top><tt>ftn_reply</tt><td>FidoNet FTS-9 Reply-ID"
	"<tr><td align=top><tt>ftn_area</tt><td>FidoNet FTS-4 echomail AREA tag"
	"<tr><td align=top><tt>ftn_flags</tt><td>FidoNet FSC-53 FLAGS"
	"<tr><td align=top><tt>ftn_pid</tt><td>FidoNet FSC-46 Program Identifier"
	"<tr><td align=top><tt>ftn_tid</tt><td>FidoNet FSC-46 Tosser Identifier"
	"<tr><td align=top><tt>date</tt><td>RFC-822 formatted date/time"
	"<tr><td align=top><tt>attr</tt><td>Attribute bitfield"
	"<tr><td align=top><tt>auxattr</tt><td>Auxillary attribute bitfield"
	"<tr><td align=top><tt>netattr</tt><td>Network attribute bitfield"
	"<tr><td align=top><tt>when_written_time</tt><td>Date/time (in time_t format)"
	"<tr><td align=top><tt>when_written_zone</tt><td>Time zone (in SMB format)"
	"<tr><td align=top><tt>when_written_zone_offset</tt><td>Time zone in minutes east of UTC"
	"<tr><td align=top><tt>when_imported_time</tt><td>Date/time message was imported"
	"<tr><td align=top><tt>when_imported_zone</tt><td>Time zone (in SMB format)"
	"<tr><td align=top><tt>when_imported_zone_offset</tt><td>Time zone in minutes east of UTC"
	"<tr><td align=top><tt>thread_id</tt><td>Thread identifier (originating message number)"
	"<tr><td align=top><tt>thread_back</tt><td>Message number that this message is a reply to"
	"<tr><td align=top><tt>thread_next</tt><td>Message number of the next reply to the original message in this thread"
	"<tr><td align=top><tt>thread_first</tt><td>Message number of the first reply to this message"
	"<tr><td align=top><tt>field_list[].type</tt><td>Other SMB header fields (type)"
	"<tr><td align=top><tt>field_list[].data</tt><td>Other SMB header fields (data)"
	"<tr><td align=top><tt>can_read</tt><td>true if the current user can read this validated or unmoderated message"
	"</table>"
	"<br><i>New in v3.12:</i> "
	"The optional <i>client</i> argument is an instance of the <i>Client</i> class to be used for the "
	"security log header fields (e.g. sender IP address, hostname, protocol, and port).  As of version 3.16c, the "
	"global client object will be used if this is omitted."
	"<br><br><i>New in v3.12:</i> "
	"The optional <i>rcpt_list</i> is an array of objects that specifies multiple recipients "
	"for a single message (e.g. bulk e-mail). Each object in the array may include the following header properties "
	"(described above): <br>"
	"<i>to</i>, <i>to_ext</i>, <i>to_org</i>, <i>to_net_type</i>, <i>to_net_addr</i>, and <i>to_agent</i>"
	)
	,312
	},
	{"vote_msg",		js_vote_msg,		1, JSTYPE_BOOLEAN,	JSDOCSTR("object header")
	,JSDOCSTR("create a new vote in message base, the <i>header</i> object should contain the following properties:<br>"
	"<table>"
	"<tr><td align=top><tt>from</tt><td>Sender's name <i>(required)</i>"
	"<tr><td align=top><tt>from_ext</tt><td>Sender's user number (if applicable)"
	"<tr><td align=top><tt>from_net_type</tt><td>Sender's network type (default: 0 for local)"
	"<tr><td align=top><tt>from_net_addr</tt><td>Sender's network address"
	"<tr><td align=top><tt>reply_id</tt><td>The Reply-ID of the message being voted on (or specify thread_back)"
	"<tr><td align=top><tt>thread_back</tt><td>Message number of the message being voted on"
	"<tr><td align=top><tt>attr</tt><td>Should be either MSG_UPVOTE, MSG_DOWNVOTE, or MSG_VOTE (if answer to poll)"
	"</table>"
	)
	,317
	},
	{"add_poll",		js_add_poll,		1, JSTYPE_BOOLEAN,	JSDOCSTR("object header")
	,JSDOCSTR("create a new poll in message base, the <i>header</i> object should contain the following properties:<br>"
	"<table>"
	"<tr><td align=top><tt>subject</tt><td>Polling question <i>(required)</i>"
	"<tr><td align=top><tt>from</tt><td>Sender's name <i>(required)</i>"
	"<tr><td align=top><tt>from_ext</tt><td>Sender's user number (if applicable)"
	"<tr><td align=top><tt>from_net_type</tt><td>Sender's network type (default: 0 for local)"
	"<tr><td align=top><tt>from_net_addr</tt><td>Sender's network address"
	"</table>"
	)
	,317
	},
	{"close_poll",		js_close_poll,		2, JSTYPE_BOOLEAN,	JSDOCSTR("message number, user name or alias")
	,JSDOCSTR("close an existing poll")
	,317
	},
	{"how_user_voted",		js_how_user_voted,		2, JSTYPE_NUMBER,	JSDOCSTR("message number, user name or alias")
	,JSDOCSTR("Returns 0 for no votes, 1 for an up-vote, 2 for a down-vote, or in the case of a poll-response: a bit-field of votes.")
	,317
	},

	{0}
};

static JSBool js_msgbase_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret=js_SyncResolve(cx, obj, name, js_msgbase_properties, js_msgbase_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_msgbase_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_msgbase_resolve(cx, obj, JSID_VOID));
}

static JSClass js_msgbase_class = {
     "MsgBase"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_msgbase_get			/* getProperty	*/
	,js_msgbase_set			/* setProperty	*/
	,js_msgbase_enumerate	/* enumerate	*/
	,js_msgbase_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_msgbase	/* finalize		*/
};

/* MsgBase Constructor (open message base) */

static JSBool
js_msgbase_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *		obj;
	jsval *			argv=JS_ARGV(cx, arglist);
	JSString*		js_str;
	JSObject*		cfgobj;
	char*			base = NULL;
	private_t*		p;
	scfg_t*			scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));
	
	obj=JS_NewObject(cx, &js_msgbase_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if((p=(private_t*)malloc(sizeof(private_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return JS_FALSE;
	}

	memset(p,0,sizeof(private_t));
	p->smb.retry_time=scfg->smb_retry_time;

	js_str = JS_ValueToString(cx, argv[0]);
	JSSTRING_TO_MSTRING(cx, js_str, base, NULL);
	if(JS_IsExceptionPending(cx)) {
		if(base != NULL)
			free(base);
		free(p);
		return JS_FALSE;
	}
	if(base==NULL) {
		JS_ReportError(cx, "Invalid base parameter");
		free(p);
		return JS_FALSE;
	}

	p->debug=JS_FALSE;

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		free(p);
		free(base);
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for accessing message bases",310);
	js_DescribeSyncConstructor(cx,obj,"To create a new MsgBase object: "
		"<tt>var msgbase = new MsgBase('<i>code</i>')</tt><br>"
		"where <i>code</i> is a sub-board internal code, or <tt>mail</tt> for the e-mail message base");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", msgbase_prop_desc, JSPROP_READONLY);
#endif

	if(stricmp(base,"mail")==0) {
		p->smb.subnum=INVALID_SUB;
		snprintf(p->smb.file,sizeof(p->smb.file),"%s%s",scfg->data_dir,"mail");
	} else {
		for(p->smb.subnum=0;p->smb.subnum<scfg->total_subs;p->smb.subnum++) {
			if(!stricmp(scfg->sub[p->smb.subnum]->code,base))
				break;
		}
		if(p->smb.subnum<scfg->total_subs) {
			cfgobj=JS_NewObject(cx,NULL,NULL,obj);

#ifdef BUILD_JSDOCS	
			/* needed for property description alignment */
			JS_DefineProperty(cx,cfgobj,"index",JSVAL_VOID
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx,cfgobj,"grp_index",JSVAL_VOID
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
#endif

			js_CreateMsgAreaProperties(cx, scfg, cfgobj, p->smb.subnum);
#ifdef BUILD_JSDOCS
			js_DescribeSyncObject(cx,cfgobj
				,"Configuration parameters for this message area (<i>sub-boards only</i>) "
				"- <small>READ ONLY</small>"
				,310);
#endif
			JS_DefineProperty(cx,obj,"cfg",OBJECT_TO_JSVAL(cfgobj)
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
			snprintf(p->smb.file,sizeof(p->smb.file),"%s%s"
				,scfg->sub[p->smb.subnum]->data_dir,scfg->sub[p->smb.subnum]->code);
		} else { /* unknown code */
			SAFECOPY(p->smb.file,base);
			p->smb.subnum=INVALID_SUB;
		}
	}

	free(base);
	return JS_TRUE;
}

static struct JSPropertySpec js_msgbase_static_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{	"IndexPrototype"		,0	,JSPROP_ENUMERATE|JSPROP_PERMANENT,	NULL,NULL},
	{	"HeaderPrototype"		,0	,JSPROP_ENUMERATE|JSPROP_PERMANENT,	NULL,NULL},
	{0}
};

JSObject* DLLCALL js_CreateMsgBaseClass(JSContext* cx, JSObject* parent, scfg_t* cfg)
{
	JSObject*	obj;
	JSObject*	constructor;
	jsval		val;

	obj = JS_InitClass(cx, parent, NULL
		,&js_msgbase_class
		,js_msgbase_constructor
		,1	/* number of constructor args */
		,NULL /* js_msgbase_properties */
		,NULL /* js_msgbase_functions */
		,js_msgbase_static_properties,NULL);


	if(JS_GetProperty(cx, parent, js_msgbase_class.name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx,val,&constructor);
		JS_DefineObject(cx,constructor,"IndexPrototype",NULL,NULL,JSPROP_PERMANENT|JSPROP_ENUMERATE);
		JS_DefineObject(cx,constructor,"HeaderPrototype",NULL,NULL,JSPROP_PERMANENT|JSPROP_ENUMERATE);
	}

	return(obj);
}


#endif	/* JAVSCRIPT */
