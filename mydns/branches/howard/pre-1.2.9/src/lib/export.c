/**********************************************************************************************
	$Id: export.c,v 1.29 2006/01/18 22:45:51 bboy Exp $

	Outputs zone data in various formats.

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
**********************************************************************************************/

#include "util.h"

void __mydns_bind_dump_rr_default(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
					 size_t datalen, int ttl, int aux, int maxlen) {
  dns_qtype_map *map = mydns_rr_get_type_by_id(rr->type);

  printf("%-*s\t%u\tIN %-5s\t%s\n", maxlen, name, ttl, map->rr_type_name, data);
}

void __mydns_bind_dump_rr_unknown(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				  size_t datalen, int ttl, int aux, int maxlen) {
}

void __mydns_bind_dump_rr_mx(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			     size_t datalen, int ttl, int aux, int maxlen) {
  printf("%-*s\t%u\tIN %-5s\t%u %s\n", maxlen, name, ttl, "MX", aux, data);
}

void __mydns_bind_dump_rr_srv(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			      size_t datalen, int ttl, int aux, int maxlen) {
  printf("%-*s\t%u\tIN %-5s\t%u %u %u %s\n", maxlen, name, ttl, "SRV", aux, MYDNS_RR_SRV_WEIGHT(rr), MYDNS_RR_SRV_PORT(rr), data);
}

void __mydns_bind_dump_rr_rp(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			     size_t datalen, int ttl, int aux, int maxlen) {
  printf("%-*s\t%u\tIN %-5s\t%s %s\n", maxlen, name, ttl, "RP", data, MYDNS_RR_RP_TXT(rr));
}

void __mydns_bind_dump_rr_txt(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			      size_t datalen, int ttl, int aux, int maxlen) {
  unsigned char *c;
  unsigned int length = datalen;

  printf("%-*s\t%u\tIN %-5s\t\"", maxlen, name, ttl, "TXT");
  
  for (c = (unsigned char*)data; length; length--, c++) {
    if (*c == '\0') {
      printf("\" \"");
      continue;
    }
    if (*c == '"')
      printf("\\\"");
    else
      printf("%c", *c);
  }
  printf("\"\n");
}

void __mydns_tinydns_dump_rr_unknown(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				     size_t datalen, int ttl, int aux) {
}

void __mydns_tinydns_dump_rr_a(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			       size_t datalen, int ttl, int aux) {
  printf("=%s:%s:%u\n", name, data, ttl);
}

void __mydns_tinydns_dump_rr_cname(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				   size_t datalen, int ttl, int aux) {
  TINYDNS_NAMEFIX(data);
  printf("C%s:%s:%u\n", name, data, ttl);
}

void __mydns_tinydns_dump_rr_mx(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				size_t datalen, int ttl, int aux) {
  TINYDNS_NAMEFIX(data);
  printf("@%s::%s:%u:%u\n", name, data, aux, ttl);
}

void __mydns_tinydns_dump_rr_ns(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				size_t datalen, int ttl, int aux) {
  TINYDNS_NAMEFIX(data);
  printf(".%s::%s:%u\n", name, data, ttl);
}

void __mydns_tinydns_dump_rr_srv(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				 size_t datalen, int ttl, int aux) {
  /* tinydns does not natively support SRV; However, there's a patch
     (http://tinydns.org/srv-patch) to support it.  This code complies with
     its format, which is "Sfqdn:ip:x:port:weight:priority:ttl:timestamp" */
  TINYDNS_NAMEFIX(data);
  printf("S%s::%s:%u:%u:%u:%u\n", name, data, MYDNS_RR_SRV_PORT(rr), MYDNS_RR_SRV_WEIGHT(rr), aux, ttl);
}

void __mydns_tinydns_dump_rr_txt(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				 size_t datalen, int ttl, int aux) {
  char *databuf, *c, *d;
  int databuflen = datalen;
  int len = databuflen;
  databuf = ALLOCATE(databuflen, char[]);
  memset(databuf, 0, databuflen);
  memcpy(databuf, data, databuflen);

  /* Need to output colons as octal - also any other wierd chars */
  for (c = (char*)data, d = databuf; len; len--, c++) {
    if (*c == ':' || !isprint((int)(*c))) {
      char *newdatabuf;
      databuflen += 3; /* Original Length + 3 more characters */
      /* Grow by a lump usually */
      newdatabuf = REALLOCATE(databuf, ((databuflen/512)+1)*512, char[]);
      if (newdatabuf != databuf) d = &newdatabuf[d - databuf];
      databuf = newdatabuf;
      d += sprintf(d, "\\%03o", *c);
    } else
      *(d++) = *c;
  }
  *d = '\0';
  printf("'%s:%s:%u\n", name, databuf, rr->ttl);
  RELEASE(databuf);
}

/* vi:set ts=3: */
/* NEED_PO */
