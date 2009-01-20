/**********************************************************************************************
	$Id: check.c,v 1.36 2005/05/04 16:49:59 bboy Exp $

	check.c: Check for problems with the data in the database.

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

#include "named.h"
#include "util.h"

int		syntax_errors, consistency_errors;	/* Number of errors found */

int		opt_extended_check = 0;			/* Extended check? */

char *__mydns_expand_data(char *s, char *origin) {
  if (!(s[0]) || LASTCHAR(s) != '.') {
    int slen = strlen(s);

    if (*s) slen += 1;
    slen += strlen(origin);

    s = REALLOCATE(s, slen + 1, char[]);
    if (*s) strcat(s, ".");
    strcat(s, origin);
  }
  return s;
}

/**********************************************************************************************
	RRPROBLEM
	Output a string describing a problem found.
**********************************************************************************************/
void __mydns_rrproblem(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
		       const char *fmt, ...) {
  va_list ap;

  meter(0,0);

  va_start(ap, fmt);
  vprintf(fmt, ap);						/* 1. message */
  va_end(ap);
  printf("\t");

  if (soa)							/* 2. soa id */
    printf("%u\t", soa->id);
  else
    printf("-\t");

  if (rr)							/* 3. rr id */
    printf("%u\t", rr->id);
  else
    printf("-\t");

  printf("%s\t", *name ? name : "-");				/* 4. name */

  if (soa || rr)						/* 5. ttl */
    printf("%u\t", rr ? rr->ttl : soa->ttl);
  else
    printf("-\t");

  printf("%s\t", rr ? mydns_rr_get_type_by_id(rr->type)->rr_type_name : "-");		/* 6. rr type */
  printf("%s\n", (data && *data) ? data : "-");			/* 7. data */

  fflush(stdout);

  syntax_errors++;
}
/*--- rrproblem() ---------------------------------------------------------------------------*/

/**********************************************************************************************
	__MYDNS_CHECK_NAME_EXTENDED
**********************************************************************************************/
void __mydns_check_name_extended(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
				 const char *name_in, size_t namelen_in, const char *fqdn, const char *col) {
  /* XXX: Add check to detect names that we should be authoritative for but
     that do not have records */
}
/*--- __mydns_check_name_extended() -----------------------------------------------------------------*/

/**********************************************************************************************
	__MYDNS_CHECK_NAME
	Verifies that "name" is a valid name.
**********************************************************************************************/
void __mydns_check_name(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			const char *name_in, size_t namelen_in, const char *col, int is_rr, int allow_underscore) {
  char		*buf, *b, *label;
  char		*fqdn;
  int		fqdnlen;
  int		buflen;
  
  fqdnlen = strlen(name_in);
  if (is_rr && *name_in && LASTCHAR(name_in) != '.') fqdnlen += strlen(soa->origin) + 1;

  fqdn = ALLOCATE(fqdnlen + 1, char[]);
  memset(fqdn, 0, fqdnlen + 1);

  strncpy(fqdn, name_in, fqdnlen);

  /* If last character isn't '.', append the origin */
  if (is_rr && *fqdn && LASTCHAR(fqdn) != '.') {
    strcat(fqdn, ".");
    strcat(fqdn, soa->origin);
  }

  if (!strlen(fqdn))
    return __mydns_rrproblem(soa, rr, name, data, _("FQDN in `%s' is empty"), col);

  if (strlen(fqdn) > DNS_MAXNAMELEN)
    return __mydns_rrproblem(soa, rr, name, data, _("FQDN in `%s' is too long"), col);

  /* Break into labels, verifying each */
  if (strcmp(fqdn, ".")) {
    buf = STRDUP(fqdn);
    for (b = buf; (label = strsep(&b, ".")); ) {
      register int len = strlen(label);
      register char *cp;

      if (!b) {		/* Last label - should be the empty string */
	if (strlen(label))
	  __mydns_rrproblem(soa, rr, name, data, _("Last label in `%s' not the root zone"), col);
	break;
      }
      if (strcmp(label, "*")) {
	if (len > DNS_MAXLABELLEN)
	  __mydns_rrproblem(soa, rr, name, data, _("Label in `%s' is too long"), col);
	if (len < 1)
	  __mydns_rrproblem(soa, rr, name, data, _("Blank label in `%s'"), col);
	for (cp = label; *cp; cp++) {
	  if (*cp == '-' && cp == label)
	    __mydns_rrproblem(soa, rr, name, data, _("Label in `%s' begins with a hyphen"), col);
	  if (*cp == '-' && ((cp - label) == len-1))
	    __mydns_rrproblem(soa, rr, name, data, _("Label in `%s' ends with a hyphen"), col);
	  if (!isalnum((int)(*cp)) && *cp != '-') {
	    if (is_rr && *cp == '*')
	      __mydns_rrproblem(soa, rr, name, data, _("Wildcard character `%c' in `%s' not alone"), *cp, col);
	    else if (*cp == '_' && allow_underscore)
	      ;
	    else
	      __mydns_rrproblem(soa, rr, name, data, _("Label in `%s' contains illegal character `%c'"), col, *cp);
	  }
	}
      } else if (!is_rr)
	__mydns_rrproblem(soa, rr, name, data, _("Wildcard not allowed in `%s'"), col);
    }
    RELEASE(buf);
  }

  /* If extended check, do extended check */
  if (is_rr && opt_extended_check)
    __mydns_check_name_extended(soa, rr, name, data, name_in, namelen_in, fqdn, col);

  RELEASE(fqdn);
}
/*--- __mydns_check_name() ------------------------------------------------------------------------------*/

/**************************************************************************************************
	__MYDNS_CHECK_RR_A
	Expanded check for A resource record.
**************************************************************************************************/
void __mydns_check_rr_a(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			const char *a_in, size_t alen_in) {
  struct in_addr addr;

  __mydns_check_name(soa, rr, name, data, name, strlen(name), "rr.name", 1, 0);

#if ALIAS_ENABLED
  if (rr->alias == 1)
    __mydns_check_rr_cname(soa, rr, name, data, a_in, alen_in);
  else {
#endif /* ALIAS_ENABLED */
    if (inet_pton(AF_INET, a_in, (void *)&addr) <= 0)
      __mydns_rrproblem(soa, rr, name, data, _("IPv4 address in `data' is invalid"));
#if ALIAS_ENABLED
  }
#endif /* ALIAS_ENABLED */

}
/*--- __mydns_check_rr_a() --------------------------------------------------------------------------*/

/**************************************************************************************************
	__MYDNS_CHECK_RR_AAAA
	Expanded check for AAAA resource record.
**************************************************************************************************/
void __mydns_check_rr_aaaa(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			   const char *aaaa_in, size_t aaaalen_in) {
  uint8_t addr[16];

  __mydns_check_name(soa, rr, name, data, name, strlen(name), "rr.name", 1, 0);

  if (inet_pton(AF_INET6, aaaa_in, (void *)&addr) <= 0)
    __mydns_rrproblem(soa, rr, name, data, _("IPv6 address in `data' is invalid"));
}
/*--- __mydns_check_rr_aaaa() --------------------------------------------------------------------------*/

/**************************************************************************************************
	__MYDNS_CHECK_RR_CNAME
	Expanded check for CNAME resource record.
**************************************************************************************************/
void __mydns_check_rr_cname(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			    const char *cname_in, size_t cnamelen_in) {
  char *xname;
  int found = 0;

  __mydns_check_name(soa, rr, name, data, name, strlen(name), "rr.name", 1, 0);

  data = __mydns_expand_data(data, soa->origin);
  __mydns_check_name(soa, rr, name, data, cname_in, cnamelen_in, "rr.data", 1, 0);

  /* A CNAME record can't have any other type of RR data for the same name */
  xname = sql_escstr(sql, (char *)name);
  found = sql_count(sql,
		    "SELECT COUNT(*) FROM %s WHERE zone=%u AND name='%s' AND type != 'CNAME' AND type != 'ALIAS'",
		    mydns_rr_table_name, rr->zone, xname);
  RELEASE(xname);
  /* If not found that way, check short name */
  if (!found) {
    xname = STRDUP(name);
    xname = sql_escstr(sql, mydns_name_2_shortname(xname, soa->origin, 1));
    found = sql_count(sql,
		      "SELECT COUNT(*) FROM %s WHERE zone=%u AND name='%s' AND type != 'CNAME' AND type != 'ALIAS'",
		      mydns_rr_table_name, rr->zone, xname);
    RELEASE(xname);
  }

  if (found)
    __mydns_rrproblem(soa, rr, name, data, _("non-CNAME record(s) present alongside CNAME"));
}
/*--- __mydns_check_rr_cname() --------------------------------------------------------------------------*/


/**************************************************************************************************
	__MYDNS_CHECK_RR_HINFO
	Expanded check for HINFO resource record.
**************************************************************************************************/
void __mydns_check_rr_hinfo(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			    const char *hinfo_in, size_t hinfolen_in) {
  char	os[DNS_MAXNAMELEN + 1] = "", cpu[DNS_MAXNAMELEN + 1] = "";

  __mydns_check_name(soa, rr, name, data, name, strlen(name), "rr.name", 1, 0);

  if (hinfo_parse((char*)hinfo_in, cpu, os, DNS_MAXNAMELEN) < 0)
    __mydns_rrproblem(soa, rr, name, data, _("data too long in HINFO record"));
}
/*--- __mydns_check_rr_hinfo() --------------------------------------------------------------------------*/


/**************************************************************************************************
	__MYDNS_CHECK_RR_MX
	Expanded check for MX resource record.
**************************************************************************************************/
void __mydns_check_rr_mx(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			 const char *mx_in, size_t mxlen_in) {

  __mydns_check_name(soa, rr, name, data, name, strlen(name), "rr.name", 1, 0);

  mx_in = __mydns_expand_data((char*)mx_in, soa->origin);
  __mydns_check_name(soa, rr, name, data, mx_in, strlen(mx_in), "rr.data", 1, 0);
}
/*--- __mydns_check_rr_mx() --------------------------------------------------------------------------*/

/**************************************************************************************************
	__MYDNS_CHECK_RR_NAPTR
	Expanded check for NAPTR resource record.
**************************************************************************************************/
void __mydns_check_rr_naptr(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			    const char *naptr_in, size_t namelen_in) {
  char *tmp, *data_copy, *p;

  __mydns_check_name(soa, rr, name, data, name, strlen(name), "rr.name", 1, 0);

  data_copy = STRNDUP(naptr_in, namelen_in);
  p = data_copy;

  if (!strsep_quotes2(&p, &tmp))
    return __mydns_rrproblem(soa, rr, name, data, _("'order' field missing from NAPTR record"));
  RELEASE(tmp);

  if (!strsep_quotes2(&p, &tmp))
    return __mydns_rrproblem(soa, rr, name, data, _("'preference' field missing from NAPTR record"));
  RELEASE(tmp);

  if (!strsep_quotes2(&p, &tmp))
    return __mydns_rrproblem(soa, rr, name, data, _("'flags' field missing from NAPTR record"));
  RELEASE(tmp);

  if (!strsep_quotes2(&p, &tmp))
    return __mydns_rrproblem(soa, rr, name, data, _("'service' field missing from NAPTR record"));
  RELEASE(tmp);

  if (!strsep_quotes2(&p, &tmp))
    return __mydns_rrproblem(soa, rr, name, data, _("'regexp' field missing from NAPTR record"));
  RELEASE(tmp);

  if (!strsep_quotes2(&p, &tmp))
    return __mydns_rrproblem(soa, rr, name, data, _("'replacement' field missing from NAPTR record"));
  RELEASE(tmp);

  /* For now, don't check 'replacement'.. the example in the RFC even contains illegal chars */
  /* data = __mydns_expand_data(tmp, soa->origin); */
  /* __mydns_check_name(tmp, "replacement", 1, 0); */
  RELEASE(data_copy);
}
/*--- __mydns_check_rr_naptr() --------------------------------------------------------------------------*/

/**************************************************************************************************
	__MYDNS_CHECK_RR_NS
	Expanded check for NS resource record.
**************************************************************************************************/
void __mydns_check_rr_ns(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			 const char *ns_in, size_t nslen_in) {
  
  __mydns_check_name(soa, rr, name, data, name, strlen(name), "rr.name", 1, 0);

  ns_in = __mydns_expand_data((char*)ns_in, soa->origin);
  __mydns_check_name(soa, rr, name, data, ns_in, strlen(ns_in), "rr.data", 1, 0);
}
/*--- __mydns_check_rr_mx() --------------------------------------------------------------------------*/

/**************************************************************************************************
	__MYDNS_CHECK_RR_RP
	Expanded check for RP resource record.
**************************************************************************************************/
void __mydns_check_rr_rp(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			 const char *rp_in, size_t rplen_in) {
  char	*txt;

  __mydns_check_name(soa, rr, name, data, name, strlen(name), "rr.name", 1, 0);

  txt = ALLOCATE(strlen(MYDNS_RR_RP_TXT(rr)) + 1, char[]);
  strcpy(txt, MYDNS_RR_RP_TXT(rr));
  data = __mydns_expand_data(txt, soa->origin);
  __mydns_check_name(soa, rr, name, data, rp_in, rplen_in, "rr.data (mbox)", 1,0 );
  __mydns_check_name(soa, rr, name, data, txt, strlen(txt), "rr.data (txt)", 1, 0);
  RELEASE(txt);
}
/*--- __mydns_check_rr_rp() --------------------------------------------------------------------------*/

/**************************************************************************************************
	__MYDNS_CHECK_RR_SRV
	Expanded check for SRV resource record.
**************************************************************************************************/
void __mydns_check_rr_srv(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			  const char *srv_in, size_t srvlen_in) {

  __mydns_check_name(soa, rr, name, data, name, strlen(name), "rr.name", 1, 1);

}
/*--- __mydns_check_rr_srv() --------------------------------------------------------------------------*/

/**************************************************************************************************
	__MYDNS_CHECK_RR_TXT
	Expanded check for TXT resource record.
**************************************************************************************************/
void __mydns_check_rr_txt(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			  const char *txt_in, size_t txtlen_in) {

  __mydns_check_name(soa, rr, name, data, name, strlen(name), "rr.name", 1, 1);

  /*
   * Data length must be less than DNS_MAXTXTLEN
   * and each element must be less than DNS_MAXTXTELEMLEN
   */
  if (txtlen_in > DNS_MAXTXTLEN) {
    return __mydns_rrproblem(soa, rr, name, data, _("Text record length is too great"));
  }

  while (txtlen_in > 0) {
    uint16_t len = strlen(txt_in);

    if (len > DNS_MAXTXTELEMLEN)
      return __mydns_rrproblem(soa, rr, name, data, _("Text element in TXT record is too long"));
    txt_in = &(txt_in[len]);
    txtlen_in -= len;
  }
}
/*--- __mydns_check_rr_txt() --------------------------------------------------------------------------*/

void __mydns_check_rr_unknown(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			      const char *unknown_in, size_t unknownlen_in) {
  __mydns_rrproblem(soa, rr, name, data, _("Unknown/unsupported resource record type"));
}

/* vi:set ts=3: */
/* NEED_PO */
