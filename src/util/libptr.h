/**************************************************************************************************
	$Id: libptr.h,v 1.4 2005/04/20 16:49:12 bboy Exp $

	libmydns.h: Header file for the MyDNS library.

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

#ifndef _LIBPTR_H
#define _LIBPTR_H

#include "mydnsutil.h"

/* Table names */
#define	MYDNS_PTR_TABLE	"ptr"

/* Configurable table names */
extern char mydns_ptr_table_name[PATH_MAX];

/* If this is nonzero, an 'active' field is assumed to exist in the table, and
	only active rows will be loaded by mydns_*_load() */
extern int mydns_ptr_use_active;
#define mydns_set_ptr_use_active(S)	\
				(mydns_ptr_use_active = sql_iscolumn((S), mydns_ptr_table_name, "active"))

#define	MYDNS_PTR_FIELDS	"id,ip,name,ttl"

typedef struct _mydns_ptr							/* `ptr' (ip-to-name mappings) */
{
	uint32_t		id;
	uint32_t		ip;
	char			name[DNS_MAXNAMELEN+1];
	uint32_t		ttl;

	struct _mydns_ptr *next;
} MYDNS_PTR;


/* libptr.c */
extern long		mydns_ptr_count(SQL *);
extern void		mydns_set_ptr_table_name(const char *);
extern MYDNS_PTR	*mydns_parse_ptr(SQL_ROW);
extern int		mydns_ptr_load(SQL *, MYDNS_PTR **, struct in_addr *);
extern MYDNS_PTR	*mydns_ptr_dup(MYDNS_PTR *, int);
extern size_t		mydns_ptr_size(MYDNS_PTR *);
extern void		_mydns_ptr_free(MYDNS_PTR *);
#define mydns_ptr_free(p) if (p) _mydns_ptr_free(p), p = NULL

#endif /* !_LIBPTR_H */

/* vi:set ts=3: */
