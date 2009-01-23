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

#ifndef _MYDNS_LIB_REPLY_H
#define _MYDNS_LIB_REPLY_H

extern void		mydns_reply_add_additional(TASK *t, RRLIST *rrlist, datasection_t section);
extern int		__mydns_reply_add_generic_rr(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_a(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_aaaa(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_hinfo(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_mx(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_naptr(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_rp(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_soa(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_srv(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_txt(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_unexpected_type(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_unknown_type(TASK *t, RR *r, dns_qtype_map *map);

#endif
