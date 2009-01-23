/**************************************************************************************************
	$Id: named.h,v 1.65 2005/04/20 16:49:12 bboy Exp $

	Copyright (C) 2002-2005  Don Moore <bboy@bboy.net>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at Your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**************************************************************************************************/

#ifndef _MYDNS_NAMED_H
#define _MYDNS_NAMED_H

#include "mydnsutil.h"

#include "mydns.h"
#include "taskobj.h"
#include "status.h"
#include "server.h"
#include "support.h"

#include "cache.h"
#include "header.h"
#include "task.h"

#if HAVE_SYS_RESOURCE_H
#	include <sys/resource.h>
#endif

#if HAVE_SYS_WAIT_H
#	include <sys/wait.h>
#endif

#if HAVE_NETDB_H
#	include <netdb.h>
#endif

/* The alarm function runs every ALARM_INTERVAL seconds */
#define		ALARM_INTERVAL		15
#define		DNS_MAXPACKETLEN	DNS_MAXPACKETLEN_UDP


/* Maximum CNAME recursion depth */
#define	DNS_MAX_CNAME_RECURSION	25

/* Size of reply header data; that's id + DNS_HEADER + qdcount + ancount + nscount + arcount */
#define	DNS_HEADERSIZE		(SIZE16 * 6)


#define SQLESC(s,d) { \
		char *rv = alloca(strlen((s))*2+1); \
		if (!rv) Err("alloca"); \
		sql_escstr(sql, rv, s, strlen((s))); \
		d = rv; \
	}

/* This is the header offset at the start of most reply functions.
	The extra SIZE16 at the end is the RDLENGTH field in the RR's header. */
#define CUROFFSET(t) (DNS_HEADERSIZE + (t)->qdlen + (t)->rdlen + SIZE16)


#if DEBUG_ENABLED
extern char *datasection_str[];			/* Strings describing data section types */
#endif

/* Global variables */

extern int	max_used_fd;

extern CACHE	*Cache;				/* Zone cache */
extern time_t	current_time;			/* Current time */

/* reply.c */
/* update.c */
extern taskexec_t	dns_update(TASK *);

#endif /* _MYDNS_NAMED_H */

/* vi:set ts=3: */
