/* Synchronet message base (SMB) library routines returning strings */

/* $Id: smbstr.c,v 1.29 2018/03/14 05:55:32 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
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

#include <ctype.h>		/* is*() */
#include <string.h>		/* strcpy, strcat, memset, strchr */
#include <genwrap.h> 		/* stricmp */
#include "smblib.h"

char* SMBCALL smb_hfieldtype(uint16_t type)
{
	static char str[8];

	switch(type) {
		case SENDER:			return("Sender");				/* RFC-compliant */
		case SENDERAGENT:		return("SenderAgent");
		case SENDERNETTYPE:		return("SenderNetType");
		case SENDERNETADDR:		return("SenderNetAddr");
		case SENDEREXT:			return("SenderExt");
		case SENDERORG:			return("Organization");			/* RFC-compliant */
		case SENDERIPADDR:		return("SenderIpAddr");
		case SENDERHOSTNAME:	return("SenderHostName");
		case SENDERPROTOCOL:	return("SenderProtocol");
		case SENDERPORT_BIN:	return("SenderPortBin");
		case SENDERPORT:		return("SenderPort");
		case SENDERUSERID:		return("SenderUserID");
		case SENDERTIME:		return("SenderTime");
		case SENDERSERVER:		return("SenderServer");

		case REPLYTO:			return("Reply-To");				/* RFC-compliant */
		case REPLYTOAGENT:		return("Reply-ToAgent");
		case REPLYTONETTYPE:	return("Reply-ToNetType");
		case REPLYTONETADDR:	return("Reply-ToNetAddr");
		case REPLYTOEXT:		return("Reply-ToExt");
								
		case RECIPIENT:			return("To");					/* RFC-compliant */
		case RECIPIENTAGENT:	return("ToAgent");
		case RECIPIENTNETTYPE:	return("ToNetType");
		case RECIPIENTNETADDR:	return("ToNetAddr");
		case RECIPIENTEXT:		return("ToExt");

		case SUBJECT:			return("Subject");				/* RFC-compliant */
		case SMB_SUMMARY:		return("Summary");
		case SMB_COMMENT:		return("Comment");				/* RFC-compliant */
		case SMB_CARBONCOPY:	return("CC");					/* RFC-compliant */
		case SMB_GROUP:			return("Group");
		case SMB_EXPIRATION:	return("Expiration");
		case SMB_PRIORITY:		return("Priority");
		case SMB_COST:			return("Cost");
		case SMB_EDITOR:		return("Editor");
		case FORWARDED:			return("Forwarded");

		/* All X-FTN-* are RFC-compliant */
		case FIDOCTRL:			return("X-FTN-Kludge");
		case FIDOAREA:			return("X-FTN-AREA");
		case FIDOSEENBY:		return("X-FTN-SEEN-BY");
		case FIDOPATH:			return("X-FTN-PATH");
		case FIDOMSGID:			return("X-FTN-MSGID");
		case FIDOREPLYID:		return("X-FTN-REPLY");
		case FIDOPID:			return("X-FTN-PID");
		case FIDOFLAGS:			return("X-FTN-Flags");
		case FIDOTID:			return("X-FTN-TID");

		case RFC822HEADER:		return("OtherHeader");
		case RFC822MSGID:		return("Message-ID");			/* RFC-compliant */
		case RFC822REPLYID:		return("In-Reply-To");			/* RFC-compliant */
		case RFC822TO:			return("RFC822To");
		case RFC822FROM:		return("RFC822From");
		case RFC822REPLYTO:		return("RFC822ReplyTo");

		case USENETPATH:		return("Path");					/* RFC-compliant */
		case USENETNEWSGROUPS:	return("Newsgroups");			/* RFC-compliant */

		case SMTPCOMMAND:		return("SMTPCommand");
		case SMTPREVERSEPATH:	return("Return-Path");			/* RFC-compliant */
		case SMTPFORWARDPATH:	return("SMTPForwardPath");
		case SMTPRECEIVED:		return("Received");				/* RFC-compliant */

		case SMTPSYSMSG:		return("SMTPSysMsg");

		case SMB_POLL_ANSWER:	return("PollAnswer");

		case UNKNOWN:			return("UNKNOWN");
		case UNKNOWNASCII:		return("UNKNOWNASCII");
		case UNUSED:			return("UNUSED");
	}
	sprintf(str,"%02Xh",type);
	return(str);
}

uint16_t SMBCALL smb_hfieldtypelookup(const char* str)
{
	uint16_t type;

	if(isdigit(*str))
		return((uint16_t)strtol(str,NULL,0));

	for(type=0;type<=UNUSED;type++)
		if(stricmp(str,smb_hfieldtype(type))==0)
			return(type);

	return(UNKNOWN);
}

char* SMBCALL smb_dfieldtype(uint16_t type)
{
	static char str[8];

	switch(type) {
		case TEXT_BODY: return("TEXT_BODY");
		case TEXT_TAIL: return("TEXT_TAIL");
		case UNUSED:	return("UNUSED");
	}
	sprintf(str,"%02Xh",type);
	return(str);
}

char* SMBCALL smb_hashsourcetype(uchar type)
{
	static char str[8];

	switch(type) {
		case SMB_HASH_SOURCE_BODY:		return(smb_dfieldtype(TEXT_BODY));
		case SMB_HASH_SOURCE_MSG_ID:	return(smb_hfieldtype(RFC822MSGID));
		case SMB_HASH_SOURCE_FTN_ID:	return(smb_hfieldtype(FIDOMSGID));
		case SMB_HASH_SOURCE_SUBJECT:	return(smb_hfieldtype(SUBJECT));
	}
	sprintf(str,"%02Xh",type);
	return(str);
}

char* SMBCALL smb_hashsource(smbmsg_t* msg, int source)
{
	switch(source) {
		case SMB_HASH_SOURCE_MSG_ID:
			return(msg->id);
		case SMB_HASH_SOURCE_FTN_ID:
			return(msg->ftn_msgid);
		case SMB_HASH_SOURCE_SUBJECT:
			return(msg->subj);
	}
	return("hash");
}

/****************************************************************************/
/* Converts when_t.zone into ASCII format                                   */
/****************************************************************************/
char* SMBCALL smb_zonestr(int16_t zone, char* str)
{
	char*		plus;
    static char buf[32];

	if(str==NULL)
		str=buf;
	switch((uint16_t)zone) {
		case 0:     return("UTC");
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
		case WET:   return("WET");
		case WEST:  return("WEST");
		case CET:   return("CET");
		case CEST:	return("CEST");
		case EET:   return("EET");
		case EEST:  return("EEST");
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

	if(!OTHER_ZONE(zone)) {
		if(zone&(WESTERN_ZONE|US_ZONE))	/* West of UTC? */
			zone=-(zone&0xfff);
		else
			zone&=0xfff;
	}

	if(zone>0)
		plus="+";
	else
		plus="";
	sprintf(str,"UTC%s%d:%02u", plus, zone/60, zone<0 ? (-zone)%60 : zone%60);

	return(str);
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char* SMBCALL smb_faddrtoa(fidoaddr_t* addr, char* str)
{
	static char buf[64];
    char point[25];

	if(addr==NULL)
		return("0:0/0");
	if(str==NULL)
		str=buf;
	sprintf(str,"%hu:%hu/%hu",addr->zone,addr->net,addr->node);
	if(addr->point) {
		sprintf(point,".%hu",addr->point);
		strcat(str,point); 
	}
	return(str);
}

/****************************************************************************/
/* Returns the FidoNet address parsed from str.								*/
/****************************************************************************/
fidoaddr_t SMBCALL smb_atofaddr(const fidoaddr_t* sys_addr, const char *str)
{
	char*		p;
	const char*	terminator;
	fidoaddr_t addr;
	fidoaddr_t tmp_addr={1,1,1,0};	/* Default system address: 1:1/1.0 */

	if(sys_addr==NULL)
		sys_addr=&tmp_addr;

	ZERO_VAR(addr);
	terminator = str;
	FIND_WHITESPACE(terminator);
	if((p=strchr(str,':'))!=NULL && p < terminator) {
		addr.zone=atoi(str);
		addr.net=atoi(p+1); 
	} else {
		addr.zone=sys_addr->zone;
		addr.net=atoi(str);
	}
	if(addr.zone==0)              /* no such thing as zone 0 */
		addr.zone=1;
	if((p=strchr(str,'/'))!=NULL && p < terminator)
		addr.node=atoi(p+1);
	else {
		if(addr.zone==sys_addr->zone)
			addr.net=sys_addr->net;
		addr.node=atoi(str); 
	}
	if((p=strchr(str,'.'))!=NULL && p < terminator)
		addr.point=atoi(p+1);
	return(addr);
}

/****************************************************************************/
/* Returns ASCIIZ representation of network address (net_t)					*/
/* NOT THREAD-SAFE!															*/
/****************************************************************************/
char* SMBCALL smb_netaddr(net_t* net)
{
	return(smb_netaddrstr(net, NULL));
}

/****************************************************************************/
/* Copies ASCIIZ representation of network address (net_t) into buf			*/
/****************************************************************************/
char* SMBCALL smb_netaddrstr(net_t* net, char* fidoaddr_buf)
{
	if(net->type==NET_FIDO)
		return(smb_faddrtoa((fidoaddr_t*)net->addr,fidoaddr_buf));
	return(net->addr);
}

/****************************************************************************/
/* Returns net_type for passed e-mail address (e.g. "user@addr")			*/
/* QWKnet and Internet addresses must have an '@'.							*/
/* FidoNet addresses may be in form: "user@addr" or just "addr".			*/
/****************************************************************************/
enum smb_net_type SMBCALL smb_netaddr_type(const char* str)
{
	const char*	p;

	if((p=strchr(str,'@'))==NULL)
		p = str;
	else
		p++;
	SKIP_WHITESPACE(p);
	if(*p==0)
		return(NET_UNKNOWN);

	return smb_get_net_type_by_addr(p);
}

/****************************************************************************/
/* Returns net_type for passed network address 								*/
/* The only addresses expected with an '@' are Internet/SMTP addresses		*/
/* Examples:																*/
/*	"VERT"				= NET_QWK											*/
/*	"1:103/705"			= NET_FIDO											*/
/*	"705.0"				= NET_FIDO											*/
/*	"705"				= NET_FIDO											*/
/*	"192.168.1.0"		= NET_INTERNET										*/
/*	"some.host"			= NET_INTERNET										*/
/*	"someone@anywhere"	= NET_INTERNET										*/
/*	"someone@some.host"	= NET_INTERNET										*/
/****************************************************************************/
enum smb_net_type SMBCALL smb_get_net_type_by_addr(const char* addr)
{
	const char*	p = addr;
	const char*	tp;
	char*	firstdot;
	char*	lastdot;

	if(strchr(p,'@') != NULL)
		return(NET_INTERNET);

	firstdot=strchr(p,'.');
	lastdot=strrchr(p,'.');

	if(isalpha(*p) && firstdot==NULL)
		return(NET_QWK);

	for(tp=p;*tp;tp++) {
		if(!isdigit(*tp) && *tp!=':' && *tp!='/' && *tp!='.')
			break;
	}
	if(isdigit(*p) && *tp==0 && firstdot==lastdot)
		return(NET_FIDO);
	if(isalnum(*p))
		return(NET_INTERNET);

	return(NET_UNKNOWN);
}

char* SMBCALL smb_nettype(enum smb_net_type type)
{
	switch(type) {
		case NET_NONE:		return "NONE";
		case NET_UNKNOWN:	return "UNKNOWN";
		case NET_QWK:		return "QWKnet";
		case NET_FIDO:		return "Fidonet";
		case NET_INTERNET:	return "Internet";
		default:			return "Unsupported net type";
	}
}
