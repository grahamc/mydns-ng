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

#ifndef _MYDNS_LIB_IMPORT_H
#define _MYDNS_LIB_IMPORT_H

extern uint32_t import_soa(const char *origin, const char *ns, const char *mbox,
			   unsigned serial, unsigned refresh, unsigned retry, unsigned expire,
			   unsigned minimum, unsigned ttl);
extern void import_rr(const char *name, const char *type, const char *data,
		      size_t datalen, unsigned aux, unsigned ttl);

extern char *__mydns_process_axfr_default(char *rv, char *name, char *origin,
					  char *reply, size_t replylen, char *src, uint32_t ttl,
					  dns_qtype_mapp map);
extern char *__mydns_process_axfr_a(char *rv, char *name, char *origin,
				    char *reply, size_t replylen, char *src, uint32_t ttl,
				    dns_qtype_mapp map);
extern char *__mydns_process_axfr_aaaa(char *rv, char *name, char *origin,
				       char *reply, size_t replylen, char *src, uint32_t ttl,
				       dns_qtype_mapp map);
extern char *__mydns_process_axfr_cname(char *rv, char *name, char *origin,
					char *reply, size_t replylen, char *src, uint32_t ttl,
					dns_qtype_mapp map);
extern char *__mydns_process_axfr_hinfo(char *rv, char *name, char *origin,
					char *reply, size_t replylen, char *src, uint32_t ttl,
					dns_qtype_mapp map);
extern char *__mydns_process_axfr_mx(char *rv, char *name, char *origin,
				     char *reply, size_t replylen, char *src, uint32_t ttl,
				     dns_qtype_mapp map);
extern char *__mydns_process_axfr_ns(char *rv, char *name, char *origin,
				     char *reply, size_t replylen, char *src, uint32_t ttl,
				     dns_qtype_mapp map);
extern char *__mydns_process_axfr_ptr(char *rv, char *name, char *origin,
				      char *reply, size_t replylen, char *src, uint32_t ttl,
				      dns_qtype_mapp map);
extern char *__mydns_process_axfr_rp(char *rv, char *name, char *origin,
				     char *reply, size_t replylen, char *src, uint32_t ttl,
				     dns_qtype_mapp map);
extern char *__mydns_process_axfr_soa(char *rv, char *name, char *origin,
				      char *reply, size_t replylen, char *src, uint32_t ttl,
				      dns_qtype_mapp map);
extern char *__mydns_process_axfr_srv(char *rv, char *name, char *origin,
				      char *reply, size_t replylen, char *src, uint32_t ttl,
				      dns_qtype_mapp map);
extern char *__mydns_process_axfr_txt(char *rv, char *name, char *origin,
				      char *reply, size_t replylen, char *src, uint32_t ttl,
				      dns_qtype_mapp map);
#endif
