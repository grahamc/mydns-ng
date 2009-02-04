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

#ifndef _MYDNS_LIB_CHECK_H
#define _MYDNS_LIB_CHECK_H

extern void mydns_rrproblem(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
			    const char *fmt, ...) __printflike(5,6);
extern void mydns_check_name(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
			     const char *name_in, size_t namelen_in, const char *col, int is_rr, int allow_underscore);

extern void __mydns_check_rr_a(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
			       const char *a_in, size_t alen_in);
extern void __mydns_check_rr_aaaa(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
				  const char *aaaa_in, size_t aaaalen_in);
extern void __mydns_check_rr_cname(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
				   const char *cname_in, size_t cnamelen_in);
extern void __mydns_check_rr_hinfo(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
				   const char *hinfo_in, size_t hinfolen_in);
extern void __mydns_check_rr_mx(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
				const char *mx_in, size_t mxlen_in);
extern void __mydns_check_rr_naptr(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
				   const char *naptr_in, size_t naptrlen_int);
extern void __mydns_check_rr_ns(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
				const char *ns_in, size_t nslen_in);
extern void __mydns_check_rr_rp(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
				const char *rp_in, size_t rplen_in);
extern void __mydns_check_rr_srv(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
				 const char *srv_in, size_t srvlen_in);
extern void __mydns_check_rr_txt(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
				 const char *txt_in, size_t txtlen_in);
extern void __mydns_check_rr_unknown(MYDNS_SOAP soa, MYDNS_RRP rr, const char *name, char *data,
				     const char *unknown_in, size_t unknownlen_in);

#endif
