/**************************************************************************************************
	$Id: str.c,v 1.19 2005/04/20 16:43:22 bboy Exp $

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

#include "mydns.h"


/**************************************************************************************************
	MYDNS_CLASS_STR
	Returns a pointer to a static string describing the specified query/response protocol class.
**************************************************************************************************/
char *
mydns_class_str(dns_class_t c) {
  static char *buf = NULL;

  switch (c) {
  case DNS_CLASS_UNKNOWN:	return ("UNKNOWN");
  case DNS_CLASS_IN:		return ("IN");
  case DNS_CLASS_CHAOS:		return ("CHAOS");
  case DNS_CLASS_HESIOD:	return ("HESIOD");
  case DNS_CLASS_NONE:		return ("NONE");
  case DNS_CLASS_ANY:		return ("ANY");
  }
  RELEASE(buf);
  ASPRINTF(&buf, "%03d", c);

  return (buf);
}
/*--- mydns_class_str() -------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_QTYPE_STR
	Returns a pointer to a static string describing the specified query/response type.
**************************************************************************************************/
static dns_qtype_t validQtypes[] = { DNS_QTYPE_A,
				     DNS_QTYPE_NS,
				     DNS_QTYPE_CNAME,
				     DNS_QTYPE_SOA,
				     DNS_QTYPE_PTR,
				     DNS_QTYPE_HINFO,
				     DNS_QTYPE_MX,
				     DNS_QTYPE_TXT,
				     DNS_QTYPE_RP,
				     DNS_QTYPE_AAAA,
				     DNS_QTYPE_SRV,
				     DNS_QTYPE_NAPTR,
#if ALIAS_ENABLED
				     DNS_QTYPE_ALIAS
#endif
};

typedef struct { char *qtype_name; dns_qtype_t qtype_id; } qtype_mapping;

static qtype_mapping map[] = { { "A",		DNS_QTYPE_A },
			       { "A6",		DNS_QTYPE_A6 },
			       { "AAAA",	DNS_QTYPE_AAAA },
			       { "AFSDB",	DNS_QTYPE_AFSDB },
#if ALIAS_ENABLED
			       { "ALIAS",	DNS_QTYPE_ALIAS },
#endif
			       { "ANY",		DNS_QTYPE_ANY },
			       { "APL",		DNS_QTYPE_APL },
			       { "ATMA",	DNS_QTYPE_ATMA },
			       { "AXFR",	DNS_QTYPE_AXFR },

			       { "CERT",	DNS_QTYPE_CERT },
			       { "CNAME",	DNS_QTYPE_CNAME },

			       { "DHCID",	DNS_QTYPE_DHCID },
			       { "DLV",		DNS_QTYPE_DLV },
			       { "DNAME",	DNS_QTYPE_DNAME },
			       { "DNSKEY",	DNS_QTYPE_DNSKEY },
			       { "DS",		DNS_QTYPE_DS },

			       { "EID",		DNS_QTYPE_EID },

			       { "GID",		DNS_QTYPE_GID },
			       { "GPOS",	DNS_QTYPE_GPOS },

			       { "HINFO",	DNS_QTYPE_HINFO },
			       { "HIP",		DNS_QTYPE_HIP },

			       { "IPSECKEY",	DNS_QTYPE_IPSECKEY },
			       { "ISDN",	DNS_QTYPE_ISDN },
			       { "IXFR",	DNS_QTYPE_IXFR },

			       { "KEY",		DNS_QTYPE_KEY },
			       { "KX",		DNS_QTYPE_KX },

			       { "LOC",		DNS_QTYPE_LOC },

			       { "MAILA",	DNS_QTYPE_MAILA },
			       { "MAILB",	DNS_QTYPE_MAILB },
			       { "MB",		DNS_QTYPE_MB },
			       { "MD",		DNS_QTYPE_MD },
			       { "MF",		DNS_QTYPE_MF },
			       { "MG",		DNS_QTYPE_MG },
			       { "MINFO",	DNS_QTYPE_MINFO },
			       { "MR",		DNS_QTYPE_MR },
			       { "MX",		DNS_QTYPE_MX },

			       { "NAPTR",	DNS_QTYPE_NAPTR },
			       { "NIMLOC",	DNS_QTYPE_NIMLOC },
			       { "NONE",	DNS_QTYPE_NONE },
			       { "NS",		DNS_QTYPE_NS },
			       { "NSAP",	DNS_QTYPE_NSAP },
			       { "NSAP_PTR",	DNS_QTYPE_NSAP_PTR },
			       { "NSEC",	DNS_QTYPE_NSEC },
			       { "NSEC3",	DNS_QTYPE_NSEC3 },
			       { "NSEC3PARAM",	DNS_QTYPE_NSEC3PARAM },
			       { "NULL",	DNS_QTYPE_NULL },
			       { "NXT",		DNS_QTYPE_NXT },

			       { "OPT",		DNS_QTYPE_OPT },

			       { "PTR",		DNS_QTYPE_PTR },
			       { "PX",		DNS_QTYPE_PX },

			       { "RP",		DNS_QTYPE_RP },
			       { "RRSIG",	DNS_QTYPE_RRSIG },
			       { "RT",		DNS_QTYPE_RT },

			       { "SIG",		DNS_QTYPE_SIG },
			       { "SINK",	DNS_QTYPE_SINK },
			       { "SOA",		DNS_QTYPE_SOA },
			       { "SPF",		DNS_QTYPE_SPF },
			       { "SRV",		DNS_QTYPE_SRV },
			       { "SSHFP",	DNS_QTYPE_SSHFP },

			       { "TA",		DNS_QTYPE_TA },
			       { "TKEY",	DNS_QTYPE_TKEY },
			       { "TSIG",	DNS_QTYPE_TSIG },
			       { "TXT",		DNS_QTYPE_TXT },

			       { "UID",		DNS_QTYPE_UID },
			       { "UINFO",	DNS_QTYPE_UINFO },
			       { "UNSPEC",	DNS_QTYPE_UNSPEC },

			       { "WKS",		DNS_QTYPE_WKS },

			       { "X25",		DNS_QTYPE_X25 },

			       { NULL,		DNS_QTYPE_UNKNOWN }
};

static qtype_mapping **qtype2str = NULL;
static int maxQtype = DNS_QTYPE_UNKNOWN;
static int numKnownQtypes = 0;

static void
mydns_qtype_init() {
  int i;
  for ( i = 0; i < sizeof(validQtypes)/sizeof(dns_qtype_t); i++) {
    maxQtype = (maxQtype < validQtypes[i]) ? validQtypes[i] : maxQtype;
  }

  qtype2str = ALLOCATE_N(maxQtype + 1, sizeof(qtype_mapping), qtype_mapping*);
  for (i = 0; i < maxQtype; i++) {
    qtype2str[i] = (qtype_mapping*)NULL;
  }
  for (i = 0; map[i].qtype_id != DNS_QTYPE_UNKNOWN; i++) {
    numKnownQtypes = i;
    qtype2str[map[i].qtype_id] = &map[i];
  }

}

dns_qtype_t
mydns_str_qtype(char *qtypeName) {
  int idx, interval, cmp;

  idx = interval = numKnownQtypes/2;
  while (interval > 1) {
    cmp = strcasecmp(qtypeName, map[idx].qtype_name);
    if (cmp ==0) { return map[idx].qtype_id; }
    interval /= 2;
    if (cmp > 0) {
      idx -= interval;
      if (idx < 0) { idx = 0; }
    } else {
      idx += interval;
      if (idx > numKnownQtypes) { idx = numKnownQtypes - 1; }
    }
  }
  while ((cmp = strcasecmp(qtypeName, map[idx].qtype_name)) != 0) {
    if (cmp > 0) { /* No match possible */ return DNS_QTYPE_UNKNOWN; }
    idx += 1;
  }
  return map[idx].qtype_id;
}

char *
mydns_qtype_str(dns_qtype_t qtype) {
  static char *buf = NULL;

  if (!qtype2str) {
    mydns_qtype_init();
  }
  if (qtype2str[qtype]) {
    return qtype2str[qtype]->qtype_name;
  }

  switch (qtype) {
  case DNS_QTYPE_UNKNOWN:	return ("UNKNOWN");

  case DNS_QTYPE_A:		return ("A");
  case DNS_QTYPE_NS:		return ("NS");
  case DNS_QTYPE_MD:		return ("MD");
  case DNS_QTYPE_MF:		return ("MF");
  case DNS_QTYPE_CNAME:		return ("CNAME");
  case DNS_QTYPE_SOA:		return ("SOA");
  case DNS_QTYPE_MB:		return ("MB");
  case DNS_QTYPE_MG:		return ("MG");
  case DNS_QTYPE_MR:		return ("MR");
  case DNS_QTYPE_NULL:		return ("NULL");
  case DNS_QTYPE_WKS:		return ("WKS");
  case DNS_QTYPE_PTR:		return ("PTR");
  case DNS_QTYPE_HINFO:		return ("HINFO");
  case DNS_QTYPE_MINFO:		return ("MINFO");
  case DNS_QTYPE_MX:		return ("MX");
  case DNS_QTYPE_TXT:		return ("TXT");
  case DNS_QTYPE_RP:		return ("RP");
  case DNS_QTYPE_AFSDB:		return ("AFSDB");
  case DNS_QTYPE_X25:		return ("X25");
  case DNS_QTYPE_ISDN:		return ("ISDN");
  case DNS_QTYPE_RT:		return ("RT");
  case DNS_QTYPE_NSAP:		return ("NSAP");
  case DNS_QTYPE_NSAP_PTR:	return ("NSAP_PTR");
  case DNS_QTYPE_SIG:		return ("SIG");
  case DNS_QTYPE_KEY:		return ("KEY");
  case DNS_QTYPE_PX:		return ("PX");
  case DNS_QTYPE_GPOS:		return ("GPOS");
  case DNS_QTYPE_AAAA:		return ("AAAA");
  case DNS_QTYPE_LOC:		return ("LOC");
  case DNS_QTYPE_NXT:		return ("NXT");
  case DNS_QTYPE_EID:		return ("EID");
  case DNS_QTYPE_NIMLOC:	return ("NIMLOC");
  case DNS_QTYPE_SRV:		return ("SRV");
  case DNS_QTYPE_ATMA:		return ("ATMA");
  case DNS_QTYPE_NAPTR:		return ("NAPTR");
  case DNS_QTYPE_KX:		return ("KX");
  case DNS_QTYPE_CERT:		return ("CERT");
  case DNS_QTYPE_A6:		return ("A6");
  case DNS_QTYPE_DNAME:		return ("DNAME");
  case DNS_QTYPE_SINK:		return ("SINK");
  case DNS_QTYPE_OPT:		return ("OPT");
  case DNS_QTYPE_APL:		return ("APL");
  case DNS_QTYPE_DS:		return ("DS");
  case DNS_QTYPE_SSHFP:		return ("SSHFP");
  case DNS_QTYPE_IPSECKEY:	return ("IPSECKEY");
  case DNS_QTYPE_RRSIG:		return ("RRSIG");
  case DNS_QTYPE_NSEC:		return ("NSEC");
  case DNS_QTYPE_DNSKEY:	return ("DNSKEY");
  case DNS_QTYPE_DHCID:		return ("DHCID");
  case DNS_QTYPE_NSEC3:		return ("NSEC");
  case DNS_QTYPE_NSEC3PARAM:	return ("NSEC3PARAM");

  case DNS_QTYPE_HIP:		return ("HIP");

  case DNS_QTYPE_SPF:		return ("SPF");
  case DNS_QTYPE_UINFO:		return ("UINFO");
  case DNS_QTYPE_UID:		return ("UID");
  case DNS_QTYPE_GID:		return ("GID");
  case DNS_QTYPE_UNSPEC:	return ("UNSPEC");

  case DNS_QTYPE_TKEY:		return ("TKEY");
  case DNS_QTYPE_TSIG:		return ("TSIG");
  case DNS_QTYPE_IXFR:		return ("IXFR");
  case DNS_QTYPE_AXFR:		return ("AXFR");
  case DNS_QTYPE_MAILB:		return ("MAILB");
  case DNS_QTYPE_MAILA:		return ("MAILA");
  case DNS_QTYPE_ANY:		return ("ANY");

  case DNS_QTYPE_TA:		return ("TA");
  case DNS_QTYPE_DLV:		return ("DLV");

#if ALIAS_ENABLED
  case DNS_QTYPE_ALIAS:		return ("ALIAS");
#endif

  case DNS_QTYPE_NONE:		break;
  }

  RELEASE(buf);
  ASPRINTF(&buf, "%03d", qtype);

  return (buf);
}
/*--- mydns_qtype_str() -------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_OPCODE_STR
	Returns a pointer to a static string describing the specified query/response opcode.
**************************************************************************************************/
char *
mydns_opcode_str(dns_opcode_t opcode) {
  static char *buf = NULL;

  switch (opcode) {
  case DNS_OPCODE_UNKNOWN:	return ("UNKNOWN");

  case DNS_OPCODE_QUERY:	return ("QUERY");
  case DNS_OPCODE_IQUERY:	return ("IQUERY");
  case DNS_OPCODE_STATUS:	return ("STATUS");
  case DNS_OPCODE_NOTIFY:	return ("NOTIFY");
  case DNS_OPCODE_UPDATE:	return ("UPDATE");
  
  }

  RELEASE(buf);
  ASPRINTF(&buf, "%03d", opcode);
  
  return (buf);
}
/*--- mydns_opcode_str() ------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_RCODE_STR
	Returns a pointer to a static string describing the specified return code.
**************************************************************************************************/
char *
mydns_rcode_str(dns_rcode_t rcode) {
  static char *buf = NULL;

  switch (rcode) {
  case DNS_RCODE_UNKNOWN:	return ("UNKNOWN");

  case DNS_RCODE_NOERROR:	return ("NOERROR");
  case DNS_RCODE_FORMERR:	return ("FORMERR");
  case DNS_RCODE_SERVFAIL:	return ("SERVFAIL");
  case DNS_RCODE_NXDOMAIN:	return ("NXDOMAIN");
  case DNS_RCODE_NOTIMP:	return ("NOTIMP");
  case DNS_RCODE_REFUSED:	return ("REFUSED");
  case DNS_RCODE_YXDOMAIN:	return ("YXDOMAIN");
  case DNS_RCODE_YXRRSET:	return ("YXRRSET");
  case DNS_RCODE_NXRRSET:	return ("NXRRSET");
  case DNS_RCODE_NOTAUTH:	return ("NOTAUTH");
  case DNS_RCODE_NOTZONE:	return ("NOTZONE");

  case DNS_RCODE_BADSIG:	return ("BADSIG");
  case DNS_RCODE_BADKEY:	return ("BADKEY");
  case DNS_RCODE_BADTIME:	return ("BADTIME");
  case DNS_RCODE_BADMODE:	return ("BADMODE");
  case DNS_RCODE_BADNAME:	return ("BADNAME");
  case DNS_RCODE_BADALG:	return ("BADALG");
  case DNS_RCODE_BADTRUNC:	return ("BADTRUNC");
  }

  RELEASE(buf);
  ASPRINTF(&buf, "%03d", rcode);

  return (buf);
}
/*--- mydns_rcode_str() -------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_SECTION_STR
	Returns a pointer to a static string describing the specified data section.
**************************************************************************************************/
char *
mydns_section_str(datasection_t section) {

  switch (section) {
  case QUESTION:		return ("QUESTION");
  case ANSWER:			return ("ANSWER");
  case AUTHORITY:		return ("AUTHORITY");
  case ADDITIONAL:		return ("ADDITIONAL");
  }
	
  return ("UNKNOWN");
}
/*--- mydns_section_str() -----------------------------------------------------------------------*/


/**************************************************************************************************
	HINFO_PARSE
	Parses an HINFO data string (CPU OS) into CPU and OS, obeying quotes and escaped characters.
	This function is called by reply_add_hinfo().
	Returns 0 on success, or -1 if an error (buffer overflow) occurred.
**************************************************************************************************/
int
hinfo_parse(char *hinfo, char *cpu, char *os, size_t destlen) {
  register char	*s, *d;
  register int	cpu_done = 0;			/* Completed 'cpu' yet? */
  register char	quote = 0;			/* If inside quotes, which quote char */

  for (s = hinfo, d = cpu; *s; s++) {
    if (!cpu_done && ((d - cpu) > (destlen - 1)))	/* Have we exceeded length for 'cpu'? */
      return (-1);
    if (cpu_done && ((d - os) > (destlen - 1)))		/* Have we exceeded length for 'os'? */
      return (-1);

    if (!cpu_done && isspace(*s) && !quote) {		/* Time to move from 'cpu' to 'os'? */
      *d = '\0';
      d = os;
      cpu_done = 1;
    } else if (*s == '\'' || *s == '"') {
      if (!quote)
	quote = *s;
      else if (quote == *s)
	quote = 0;
      else
	*d++ = *s;
    } else
      *d++ = *s;
  }
  return (0);
}
/*--- hinfo_parse() -----------------------------------------------------------------------------*/

/* vi:set ts=3: */
