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

#ifndef _MYDNS_LIB_EXPORT_H
#define _MYDNS_LIB_EXPORT_H

void __mydns_bind_dump_rr_unknown(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				  size_t datalen, int ttl, int aux, int maxlen);
void __mydns_bind_dump_rr_default(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				  size_t datalen, int ttl, int aux, int maxlen);
void __mydns_bind_dump_rr_mx(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			     size_t datalen, int ttl, int aux, int maxlen);
void __mydns_bind_dump_rr_rp(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			     size_t datalen, int ttl, int aux, int maxlen);
void __mydns_bind_dump_rr_srv(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			      size_t datalen, int ttl, int aux, int maxlen);
void __mydns_bind_dump_rr_txt(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			      size_t datalen, int ttl, int aux, int maxlen);

void __mydns_tinydns_dump_rr_unknown(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				     size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_a(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			       size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_cname(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				   size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_mx(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_ns(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_srv(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				 size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_txt(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				 size_t datalen, int ttl, int aux);
#endif
