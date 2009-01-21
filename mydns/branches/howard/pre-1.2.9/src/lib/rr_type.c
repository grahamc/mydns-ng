/**************************************************************************************************
	$Id: rr.c,v 1.65 2005/04/29 16:10:27 bboy Exp $

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

#include "named.h"

#define DEFINE_RR_TYPE(__TYPENAME__, __PERSISTENT__, __PARSER__, __FREE__, __DUPLICATOR__, \
		       __SIZOR__, __REPLY_ADD__, __UPDATE_GET_RR_DATA__, __PROCESS_AXFR__, \
		       __CHECK_RR__, \
		       __EXPORT_BIND_RR__, __EXPORT_TINYDNS_RR__, \
		       __UPDATE_ENABLED__,				\
		       __MATCH_AUX__, __WHERECLAUSE__)			\
  static dns_qtype_map __RR_TYPE_##__TYPENAME__ = { "" #__TYPENAME__ "", \
						    DNS_QTYPE_##__TYPENAME__, \
						    __PERSISTENT__, \
						    __PARSER__, \
						    __FREE__, \
						    __DUPLICATOR__, \
						    __SIZOR__, \
						    __REPLY_ADD__, \
						    __UPDATE_GET_RR_DATA__, \
						    __PROCESS_AXFR__, \
						    __CHECK_RR__, \
						    __EXPORT_BIND_RR__, \
						    __EXPORT_TINYDNS_RR__, \
						    __UPDATE_ENABLED__, \
						    __MATCH_AUX__, \
						    __WHERECLAUSE__ }

#define USE_RR_TYPE(__TYPENAME__) \
  &__RR_TYPE_##__TYPENAME__

DEFINE_RR_TYPE(A6, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	       __mydns_rr_duplicate_default, __mydns_rr_size_default, \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,	\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(AAAA, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	       __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_add_aaaa, __mydns_update_get_rr_data_aaaa, \
	       __mydns_process_axfr_aaaa, __mydns_check_rr_aaaa,				\
	       __mydns_bind_dump_rr_default, __mydns_tinydns_dump_rr_unknown, \
	       1, 0, NULL);

#if ALIAS_ENABLED
DEFINE_RR_TYPE(A, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	       __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_add_a, __mydns_update_get_rr_data_a, \
	       __mydns_process_axfr_a, __mydns_check_rr_a,			  \
	       __mydns_bind_dump_rr_default, __mydns_tinydns_dump_rr_a, \
	       1, 0, " AND (type='A' OR type='ALIAS')");
#else
DEFINE_RR_TYPE(A, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	       __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_add_a, __mydns_update_get_rr_data_a, \
	       __mydns_process_axfr_a, __mydns_check_rr_a,				\
	       __mydns_bind_dump_rr_default, __mydns_tinydns_dump_rr_a, \
	       1, 0, NULL);
#endif

DEFINE_RR_TYPE(AFSDB, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	       __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

#if ALIAS_ENABLED
DEFINE_RR_TYPE(ALIAS, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	       __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unexpected_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);
#endif

DEFINE_RR_TYPE(ANY, 0, __mydns_rr_parse_default, __mydns_rr_free_default, \
	       __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unexpected_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, "");

DEFINE_RR_TYPE(APL, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	       __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(ATMA, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	       __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(AXFR, 0, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unexpected_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(CERT, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(CNAME, 1, __mydns_rr_parse_cname, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_add_generic_rr, __mydns_update_get_rr_data_cname, \
	       __mydns_process_axfr_cname, __mydns_check_rr_cname,				\
	       __mydns_bind_dump_rr_default, __mydns_tinydns_dump_rr_cname, \
	       1, 0, NULL);

DEFINE_RR_TYPE(DHCID, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(DLV, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(DNAME, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(DNSKEY, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(DS, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(EID, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(GID, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(GPOS, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(HINFO, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_add_hinfo, __mydns_update_get_rr_data_hinfo, \
	       __mydns_process_axfr_hinfo, __mydns_check_rr_hinfo,				\
	       __mydns_bind_dump_rr_default, __mydns_tinydns_dump_rr_unknown, \
	       1, 0, NULL);

DEFINE_RR_TYPE(HIP, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(IPSECKEY, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(ISDN, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(IXFR, 0, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unexpected_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(KEY, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(KX, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(LOC, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(MAILA, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(MAILB, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(MB, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(MD, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(MF, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(MG, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(MINFO, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(MR, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(MX, 1, __mydns_rr_parse_mx, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_add_mx, __mydns_update_get_rr_data_mx, \
	       __mydns_process_axfr_mx, __mydns_check_rr_mx,				\
	       __mydns_bind_dump_rr_mx, __mydns_tinydns_dump_rr_mx, \
	       1, 1, NULL);

DEFINE_RR_TYPE(NAPTR, 1, __mydns_rr_parse_naptr, __mydns_rr_free_naptr, \
	        __mydns_rr_duplicate_naptr, __mydns_rr_size_naptr,  \
	       __mydns_reply_add_naptr, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_naptr,				\
	       __mydns_bind_dump_rr_default, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(NIMLOC, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(NONE, 0, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unexpected_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(NSAP, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(NSAP_PTR, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(NS, 1, __mydns_rr_parse_ns, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_add_generic_rr, __mydns_update_get_rr_data_ns, \
	       __mydns_process_axfr_ns, __mydns_check_rr_ns,				\
	       __mydns_bind_dump_rr_default, __mydns_tinydns_dump_rr_ns, \
	       1, 0, NULL);

DEFINE_RR_TYPE(NSEC3, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(NSEC3PARAM, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(NSEC, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(NULL, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(NXT, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(OPT, 0, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(PTR, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_add_generic_rr, __mydns_update_get_rr_data_ptr, \
	       __mydns_process_axfr_ptr, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_default, __mydns_tinydns_dump_rr_unknown, \
	       1, 0, NULL);

DEFINE_RR_TYPE(PX, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(RP, 1, __mydns_rr_parse_rp, __mydns_rr_free_rp, \
	        __mydns_rr_duplicate_rp, __mydns_rr_size_rp,  \
	       __mydns_reply_add_rp, __mydns_update_get_rr_data_rp, \
	       __mydns_process_axfr_rp, __mydns_check_rr_rp,				\
	       __mydns_bind_dump_rr_rp, __mydns_tinydns_dump_rr_unknown, \
	       1, 0, NULL);

DEFINE_RR_TYPE(RRSIG, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(RT, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(SIG, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(SINK, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(SOA, 0, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_add_soa, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_soa, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(SPF, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(SRV, 1, __mydns_rr_parse_srv, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_srv, __mydns_rr_size_default,  \
	       __mydns_reply_add_srv, __mydns_update_get_rr_data_srv, \
	       __mydns_process_axfr_srv, __mydns_check_rr_srv,				\
	       __mydns_bind_dump_rr_srv, __mydns_tinydns_dump_rr_srv, \
	       1, 1, NULL);

DEFINE_RR_TYPE(SSHFP, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(TA, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(TKEY, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(TSIG, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(TXT, 1, __mydns_rr_parse_txt, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_add_txt, __mydns_update_get_rr_data_txt, \
	       __mydns_process_axfr_txt, __mydns_check_rr_txt,				\
	       __mydns_bind_dump_rr_txt, __mydns_tinydns_dump_rr_txt, \
	       1, 0, NULL);

DEFINE_RR_TYPE(UID, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(UINFO, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(UNSPEC, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(WKS, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

DEFINE_RR_TYPE(X25, 1, __mydns_rr_parse_default, __mydns_rr_free_default, \
	        __mydns_rr_duplicate_default, __mydns_rr_size_default,  \
	       __mydns_reply_unknown_type, __mydns_update_get_rr_data_unknown_type, \
	       __mydns_process_axfr_default, __mydns_check_rr_unknown,				\
	       __mydns_bind_dump_rr_unknown, __mydns_tinydns_dump_rr_unknown, \
	       0, 0, NULL);

static dns_qtype_map *__RR_TYPES_BY_ID[] = {
  USE_RR_TYPE(NONE),
  USE_RR_TYPE(A),
  USE_RR_TYPE(NS),
  USE_RR_TYPE(MD),
  USE_RR_TYPE(MF),
  USE_RR_TYPE(CNAME),
  USE_RR_TYPE(SOA),
  USE_RR_TYPE(MB),
  USE_RR_TYPE(MG),
  USE_RR_TYPE(MR),
  USE_RR_TYPE(NULL),
  USE_RR_TYPE(WKS),
  USE_RR_TYPE(PTR),
  USE_RR_TYPE(HINFO),
  USE_RR_TYPE(MINFO),
  USE_RR_TYPE(MX),
  USE_RR_TYPE(TXT),
  USE_RR_TYPE(RP),
  USE_RR_TYPE(AFSDB),
  USE_RR_TYPE(X25),
  USE_RR_TYPE(ISDN),
  USE_RR_TYPE(RT),
  USE_RR_TYPE(NSAP),
  USE_RR_TYPE(NSAP_PTR),
  USE_RR_TYPE(SIG),
  USE_RR_TYPE(KEY),
  USE_RR_TYPE(PX),
  USE_RR_TYPE(GPOS),
  USE_RR_TYPE(AAAA),
  USE_RR_TYPE(LOC),
  USE_RR_TYPE(NXT),
  USE_RR_TYPE(EID),
  USE_RR_TYPE(NIMLOC),
  USE_RR_TYPE(SRV),
  USE_RR_TYPE(ATMA),
  USE_RR_TYPE(NAPTR),
  USE_RR_TYPE(KX),
  USE_RR_TYPE(CERT),
  USE_RR_TYPE(A6),
  USE_RR_TYPE(DNAME),
  USE_RR_TYPE(SINK),
  USE_RR_TYPE(OPT),
  USE_RR_TYPE(APL),
  USE_RR_TYPE(DS),
  USE_RR_TYPE(SSHFP),
  USE_RR_TYPE(IPSECKEY),
  USE_RR_TYPE(RRSIG),
  USE_RR_TYPE(NSEC),
  USE_RR_TYPE(DNSKEY),
  USE_RR_TYPE(DHCID),
  USE_RR_TYPE(NSEC3),
  USE_RR_TYPE(NSEC3PARAM),
  USE_RR_TYPE(HIP),
  USE_RR_TYPE(SPF),
  USE_RR_TYPE(UINFO),
  USE_RR_TYPE(UID),
  USE_RR_TYPE(GID),
  USE_RR_TYPE(UNSPEC),
  USE_RR_TYPE(TKEY),
  USE_RR_TYPE(TSIG),
  USE_RR_TYPE(IXFR),
  USE_RR_TYPE(AXFR),
  USE_RR_TYPE(MAILB),
  USE_RR_TYPE(MAILA),
  USE_RR_TYPE(ANY),
  USE_RR_TYPE(TA),
  USE_RR_TYPE(DLV),
  USE_RR_TYPE(ALIAS)
};

static dns_qtype_map *__RR_TYPES_BY_NAME[] = {
  USE_RR_TYPE(A6),
  USE_RR_TYPE(AAAA),
  USE_RR_TYPE(A),
  USE_RR_TYPE(AFSDB),
#if ALIAS_ENABLED
  USE_RR_TYPE(ALIAS),
#endif
  USE_RR_TYPE(ANY),
  USE_RR_TYPE(APL),
  USE_RR_TYPE(ATMA),
  USE_RR_TYPE(AXFR),
  USE_RR_TYPE(CERT),
  USE_RR_TYPE(CNAME),
  USE_RR_TYPE(DHCID),
  USE_RR_TYPE(DLV),
  USE_RR_TYPE(DNAME),
  USE_RR_TYPE(DNSKEY),
  USE_RR_TYPE(DS),
  USE_RR_TYPE(EID),
  USE_RR_TYPE(GID),
  USE_RR_TYPE(GPOS),
  USE_RR_TYPE(HINFO),
  USE_RR_TYPE(HIP),
  USE_RR_TYPE(IPSECKEY),
  USE_RR_TYPE(ISDN),
  USE_RR_TYPE(IXFR),
  USE_RR_TYPE(KEY),
  USE_RR_TYPE(KX),
  USE_RR_TYPE(LOC),
  USE_RR_TYPE(MAILA),
  USE_RR_TYPE(MAILB),
  USE_RR_TYPE(MB),
  USE_RR_TYPE(MD),
  USE_RR_TYPE(MF),
  USE_RR_TYPE(MG),
  USE_RR_TYPE(MINFO),
  USE_RR_TYPE(MR),
  USE_RR_TYPE(MX),
  USE_RR_TYPE(NAPTR),
  USE_RR_TYPE(NIMLOC),
  USE_RR_TYPE(NONE),
  USE_RR_TYPE(NSAP),
  USE_RR_TYPE(NSAP_PTR),
  USE_RR_TYPE(NS),
  USE_RR_TYPE(NSEC3),
  USE_RR_TYPE(NSEC3PARAM),
  USE_RR_TYPE(NSEC),
  USE_RR_TYPE(NULL),
  USE_RR_TYPE(NXT),
  USE_RR_TYPE(OPT),
  USE_RR_TYPE(PTR),
  USE_RR_TYPE(PX),
  USE_RR_TYPE(RP),
  USE_RR_TYPE(RRSIG),
  USE_RR_TYPE(RT),
  USE_RR_TYPE(SIG),
  USE_RR_TYPE(SINK),
  USE_RR_TYPE(SOA),
  USE_RR_TYPE(SPF),
  USE_RR_TYPE(SRV),
  USE_RR_TYPE(SSHFP),
  USE_RR_TYPE(TA),
  USE_RR_TYPE(TKEY),
  USE_RR_TYPE(TSIG),
  USE_RR_TYPE(TXT),
  USE_RR_TYPE(UID),
  USE_RR_TYPE(UINFO),
  USE_RR_TYPE(UNSPEC),
  USE_RR_TYPE(WKS),
  USE_RR_TYPE(X25)
};

dns_qtype_map **RR_TYPES_BY_ID = &(__RR_TYPES_BY_ID[0]);
dns_qtype_map **RR_TYPES_BY_NAME = &(__RR_TYPES_BY_NAME[0]);
int RR_TYPES_ENTRIES = sizeof(__RR_TYPES_BY_NAME)/sizeof(__RR_TYPES_BY_NAME[0]);

/**************************************************************************************************
	MYDNS_RR_GET_TYPE_BY_ID
**************************************************************************************************/
dns_qtype_map
*mydns_rr_get_type_by_id(dns_qtype_t type) {
  register int i;

  /* Replace by a binary chop or a biased binary chop or indexed lookup */
  for (i = 0; i < RR_TYPES_ENTRIES; i++) {
    if (type == RR_TYPES_BY_ID[i]->rr_type)
      return RR_TYPES_BY_ID[i];
  }

  return NULL;
}
/*--- mydns_rr_get_type_by_name() ---------------------------------------------------------------*/

/**************************************************************************************************
	MYDNS_RR_GET_TYPE_BY_NAME
**************************************************************************************************/
dns_qtype_map
*mydns_rr_get_type_by_name(char *type) {
  register char *c;
  register int i;

  for (c = type; *c; c++)
    *c = toupper(*c);

  /* Replace by a binary chop or a biased binary chop */
  for (i = 0; i < RR_TYPES_ENTRIES; i++) {
    if (strcmp(type, RR_TYPES_BY_NAME[i]->rr_type_name) == 0)
      return RR_TYPES_BY_NAME[i];
  }

  return NULL;
}
/*--- mydns_rr_get_type_by_name() ---------------------------------------------------------------*/

