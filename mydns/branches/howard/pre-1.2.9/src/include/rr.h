/**************************************************************************************************
	Copyright (C) 2009-  Howard Wilkinson <howard@cohtech.com>

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

#ifndef _MYDNS_LIB_RR_H
#define _MYDNS_LIB_RR_H

#include "mydns.h"
#include "taskobj.h"

extern int __mydns_rr_parse_default(const char *origin, MYDNS_RRP rr);
extern int __mydns_rr_parse_cname(const char *origin, MYDNS_RRP rr);
extern int __mydns_rr_parse_mx(const char *origin, MYDNS_RRP rr);
extern int __mydns_rr_parse_naptr(const char *origin, MYDNS_RRP rr);
extern int __mydns_rr_parse_ns(const char *origin, MYDNS_RRP rr);
extern int __mydns_rr_parse_rp(const char *origin, MYDNS_RRP rr);
extern int __mydns_rr_parse_srv(const char *origin, MYDNS_RRP rr);
extern int __mydns_rr_parse_txt(const char *origin, MYDNS_RRP rr);

extern void __mydns_rr_free_default(MYDNS_RRP rr);
extern void __mydns_rr_free_naptr(MYDNS_RRP rr);
extern void __mydns_rr_free_rp(MYDNS_RRP rr);

extern void __mydns_rr_duplicate_default(MYDNS_RRP dst, MYDNS_RRP src);
extern void __mydns_rr_duplicate_naptr(MYDNS_RRP dst, MYDNS_RRP src);
extern void __mydns_rr_duplicate_rp(MYDNS_RRP dst, MYDNS_RRP src);
extern void __mydns_rr_duplicate_srv(MYDNS_RRP dst, MYDNS_RRP src);

extern size_t __mydns_rr_size_default(MYDNS_RRP rr);
extern size_t __mydns_rr_size_naptr(MYDNS_RRP rr);
extern size_t __mydns_rr_size_rp(MYDNS_RRP rr);

extern long		mydns_rr_count(SQL *);
extern void		mydns_rr_get_active_types(SQL *);
extern void		mydns_set_rr_table_name(char *);
extern void		mydns_set_rr_where_clause(char *);
extern char *		mydns_rr_append_origin(char *, char *);
extern void		mydns_rr_name_append_origin(MYDNS_RR *, char *);
extern void		mydns_rr_data_append_origin(MYDNS_RR *, char *);
extern void		_mydns_rr_free(MYDNS_RR *);
#define			mydns_rr_free(p)	if ((p)) _mydns_rr_free((p)), (p) = NULL
extern MYDNS_RR		*mydns_rr_build(uint32_t, uint32_t, dns_qtype_mapp, dns_class_t, uint32_t, uint32_t,
					char *active,
#if USE_PGSQL
					timestamp *stamp,
#else
					MYSQL_TIME *stamp,
#endif
					uint32_t, char *, char *,  uint16_t, const char *);
extern MYDNS_RR		*mydns_rr_parse(SQL_ROW, unsigned long *, const char *);
extern char		*mydns_rr_columns(void);
extern char		*mydns_rr_prepare_query(uint32_t, dns_qtype_t, char *,
						char *, char *, char *, char *);
extern int		mydns_rr_load_all(SQL *, MYDNS_RR **, uint32_t, dns_qtype_t, char *, char *);
extern int		mydns_rr_load_active(SQL *, MYDNS_RR **, uint32_t, dns_qtype_t, char *, char *);
extern int		mydns_rr_load_inactive(SQL *, MYDNS_RR **, uint32_t, dns_qtype_t, char *, char *);
extern int		mydns_rr_load_deleted(SQL *, MYDNS_RR **, uint32_t, dns_qtype_t, char *, char *);
extern int		mydns_rr_count_all(SQL *, uint32_t, dns_qtype_t, char *, char *);
extern int		mydns_rr_count_active(SQL *, uint32_t, dns_qtype_t, char *, char *);
extern int		mydns_rr_count_inactive(SQL *, uint32_t, dns_qtype_t, char *, char *);
extern int		mydns_rr_count_deleted(SQL *, uint32_t, dns_qtype_t, char *, char *);
extern int		mydns_rr_load_all_filtered(SQL *, MYDNS_RR **, uint32_t, dns_qtype_t, char *, char *, char *);
extern int		mydns_rr_load_active_filtered(SQL *, MYDNS_RR **, uint32_t, dns_qtype_t, char *, char *, char *);
extern int		mydns_rr_load_inactive_filtered(SQL *, MYDNS_RR **, uint32_t, dns_qtype_t, char *, char *, char *);
extern int		mydns_rr_load_deleted_filtered(SQL *, MYDNS_RR **, uint32_t, dns_qtype_t, char *, char *, char *);
extern int		mydns_rr_count_all_filtered(SQL *, uint32_t, dns_qtype_t, char *, char *, char *);
extern int		mydns_rr_count_active_filtered(SQL *, uint32_t, dns_qtype_t, char *, char *, char *);
extern int		mydns_rr_count_inactive_filtered(SQL *, uint32_t, dns_qtype_t, char *, char *, char *);
extern int		mydns_rr_count_deleted_filtered(SQL *, uint32_t, dns_qtype_t, char *, char *, char *);
extern MYDNS_RR		*mydns_rr_dup(MYDNS_RR *, int);
extern size_t		mydns_rr_size(MYDNS_RR *);

extern void		rrlist_add(TASK *, datasection_t, dns_rrtype_t, void *, char *);
extern void		rrlist_free(RRLIST *);

#endif
