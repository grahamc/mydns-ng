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
extern int		__mydns_reply_add_generic_rr(TASKP t, RRP r, dns_qtype_mapp map);
extern int		__mydns_reply_add_a(TASKP t, RRP r, dns_qtype_mapp map);
extern int		__mydns_reply_add_aaaa(TASKP t, RRP r, dns_qtype_mapp map);
extern int		__mydns_reply_add_hinfo(TASKP t, RRP r, dns_qtype_mapp map);
extern int		__mydns_reply_add_mx(TASKP t, RRP r, dns_qtype_mapp map);
extern int		__mydns_reply_add_naptr(TASKP t, RRP r, dns_qtype_mapp map);
extern int		__mydns_reply_add_rp(TASKP t, RRP r, dns_qtype_mapp map);
extern int		__mydns_reply_add_soa(TASKP t, RRP r, dns_qtype_mapp map);
extern int		__mydns_reply_add_srv(TASKP t, RRP r, dns_qtype_mapp map);
extern int		__mydns_reply_add_txt(TASKP t, RRP r, dns_qtype_mapp map);
extern int		__mydns_reply_unexpected_type(TASKP t, RRP r, dns_qtype_mapp map);
extern int		__mydns_reply_unknown_type(TASKP t, RRP r, dns_qtype_mapp map);

#endif
