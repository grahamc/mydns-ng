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

#include "named.h"


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
