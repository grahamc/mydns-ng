/**************************************************************************************************
	$Id: util.h,v 1.14 2005/04/20 16:49:12 bboy Exp $

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

#ifndef _MYDNS_UTIL_DIR_H
#define _MYDNS_UTIL_DIR_H

#include "mydnsutil.h"
#include "mydns.h"

/* I'm not done with tinydns-data support yet.  It won't work.
	Defining this and recompiling will NOT let you import tinydns-data files! */
#define TINYDNS_IMPORT

extern CONF *Conf;												/* Configuration data */

extern void load_config(void);
extern void db_connect(void);
extern uint32_t sqlnum(const char *, ...) __printflike(1,2);
extern void meter(unsigned long, unsigned long);

extern uint32_t import_soa(const uchar *origin, const uchar *ns, const uchar *mbox,
			   unsigned serial, unsigned refresh, unsigned retry, unsigned expire,
			   unsigned minimum, unsigned ttl);
extern void import_rr(const uchar *name, const uchar *type, const uchar *data,
		      size_t datalen, unsigned aux, unsigned ttl);

extern void import_axfr(char *hostport, char *import_zone);
#ifdef TINYDNS_IMPORT
extern void import_tinydns(char *datafile, char *import_zone);
#endif

#endif /* !_MYDNS_UTIL_DIR_H */

/* vi:set ts=3: */
