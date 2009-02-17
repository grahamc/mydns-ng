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

#include "memoryman.h"

#if DEBUG_ENABLED
int		debug_str = 0;
#endif

/**************************************************************************************************
	MYDNS_CLASS_STR
	Returns a pointer to a static string describing the specified query/response protocol class.
**************************************************************************************************/
const char *
mydns_class_str(dns_class_t c) {
  const char *buf = NULL;

#if DEBUG_ENABLED
  Debug(str, DEBUGLEVEL_FUNCS, _("mydns_class_str called"));
#endif

  switch (c) {
  case DNS_CLASS_UNKNOWN:	buf = "UNKNOWN"; break;
  case DNS_CLASS_IN:		buf = "IN"; break;
  case DNS_CLASS_CHAOS:		buf = "CHAOS"; break;
  case DNS_CLASS_HESIOD:	buf = "HESIOD"; break;
  case DNS_CLASS_NONE:		buf = "NONE"; break;
  case DNS_CLASS_ANY:		buf = "ANY"; break;
  default:
    {
      static char *msg = NULL;
      RELEASE(msg);
      ASPRINTF(&msg, "%03d", c);
      buf = (const char*)msg;
    }
    break;
  }

#if DEBUG_ENABLED
  Debug(str, DEBUGLEVEL_FUNCS, _("mydns_class_str returns %s"), buf);
#endif
  return buf;
}
/*--- mydns_class_str() -------------------------------------------------------------------------*/

/**************************************************************************************************
	MYDNS_OPCODE_STR
	Returns a pointer to a static string describing the specified query/response opcode.
**************************************************************************************************/
const char *
mydns_opcode_str(dns_opcode_t opcode) {
  const char *buf = NULL;

#if DEBUG_ENABLED
  Debug(str, DEBUGLEVEL_FUNCS, _("mydns_opcode_str called"));
#endif

  switch (opcode) {
  case DNS_OPCODE_UNKNOWN:	buf = "UNKNOWN"; break;

  case DNS_OPCODE_QUERY:	buf = "QUERY"; break;
  case DNS_OPCODE_IQUERY:	buf = "IQUERY"; break;
  case DNS_OPCODE_STATUS:	buf = "STATUS"; break;
  case DNS_OPCODE_NOTIFY:	buf = "NOTIFY"; break;
  case DNS_OPCODE_UPDATE:	buf = "UPDATE"; break;
  default:
    {
      static char *msg = NULL;
      RELEASE(msg);
      ASPRINTF(&msg, "%03d", opcode);
      buf = (const char*)msg;
    }
    break;
  }

#if DEBUG_ENABLED
  Debug(str, DEBUGLEVEL_FUNCS, _("mydns_opcode_str returns %s"), buf);
#endif
  return buf;
}
/*--- mydns_opcode_str() ------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_RCODE_STR
	Returns a pointer to a static string describing the specified return code.
**************************************************************************************************/
const char *
mydns_rcode_str(dns_rcode_t rcode) {
  const char *buf;

#if DEBUG_ENABLED
  Debug(str, DEBUGLEVEL_FUNCS, _("mydns_rcode_str called"));
#endif

  switch (rcode) {
  case DNS_RCODE_UNKNOWN:	buf = "UNKNOWN"; break;

  case DNS_RCODE_NOERROR:	buf = "NOERROR"; break;
  case DNS_RCODE_FORMERR:	buf = "FORMERR"; break;
  case DNS_RCODE_SERVFAIL:	buf = "SERVFAIL"; break;
  case DNS_RCODE_NXDOMAIN:	buf = "NXDOMAIN"; break;
  case DNS_RCODE_NOTIMP:	buf = "NOTIMP"; break;
  case DNS_RCODE_REFUSED:	buf = "REFUSED"; break;
  case DNS_RCODE_YXDOMAIN:	buf = "YXDOMAIN"; break;
  case DNS_RCODE_YXRRSET:	buf = "YXRRSET"; break;
  case DNS_RCODE_NXRRSET:	buf = "NXRRSET"; break;
  case DNS_RCODE_NOTAUTH:	buf = "NOTAUTH"; break;
  case DNS_RCODE_NOTZONE:	buf = "NOTZONE"; break;

  case DNS_RCODE_BADSIG:	buf = "BADSIG"; break;
  case DNS_RCODE_BADKEY:	buf = "BADKEY"; break;
  case DNS_RCODE_BADTIME:	buf = "BADTIME"; break;
  case DNS_RCODE_BADMODE:	buf = "BADMODE"; break;
  case DNS_RCODE_BADNAME:	buf = "BADNAME"; break;
  case DNS_RCODE_BADALG:	buf = "BADALG"; break;
  case DNS_RCODE_BADTRUNC:	buf = "BADTRUNC"; break;
  default:
    {
      static char *msg = NULL;
      RELEASE(msg);
      ASPRINTF(&msg, "%03d", rcode);
      buf = (const char*)msg;
    }
    break;
  }

#if DEBUG_ENABLED
  Debug(str, DEBUGLEVEL_FUNCS, _("mydns_rcode_str returns %s"), buf);
#endif
  return buf;
}
/*--- mydns_rcode_str() -------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_SECTION_STR
	Returns a pointer to a static string describing the specified data section.
**************************************************************************************************/
const char *
mydns_section_str(datasection_t section) {
  const char *buf = "UNKNOWN";

#if DEBUG_ENABLED
  Debug(str, DEBUGLEVEL_FUNCS, _("mydns_section_str called"));
#endif
  switch (section) {
  case QUESTION:		buf = ("QUESTION"); break;
  case ANSWER:			buf = ("ANSWER"); break;
  case AUTHORITY:		buf = ("AUTHORITY"); break;
  case ADDITIONAL:		buf = ("ADDITIONAL"); break;
  }
	
#if DEBUG_ENABLED
  Debug(str, DEBUGLEVEL_FUNCS, _("mydns_section_str returns %s"), buf);
#endif
  return buf;
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

#if DEBUG_ENABLED
  Debug(str, DEBUGLEVEL_FUNCS, _("hinfo_parse called"));
#endif
  for (s = hinfo, d = cpu; *s; s++) {
    if (!cpu_done && ((size_t)(d - cpu) > (destlen - 1))) {	/* Have we exceeded length for 'cpu'? */
#if DEBUG_ENABLED
      Debug(str, DEBUGLEVEL_FUNCS, _("hinfo_parse returns -1 CPU length is too large"));
#endif
      return (-1);
    }
    if (cpu_done && ((size_t)(d - os) > (destlen - 1))) {	/* Have we exceeded length for 'os'? */
#if DEBUG_ENABLED
      Debug(str, DEBUGLEVEL_FUNCS, _("hinfo_parse returns -1 OS length is too large"));
#endif
      return (-1);
    }

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
#if DEBUG_ENABLED
  Debug(str, DEBUGLEVEL_FUNCS, _("hinfo_parse returns 0"));
#endif
  return (0);
}
/*--- hinfo_parse() -----------------------------------------------------------------------------*/

/* vi:set ts=3: */
