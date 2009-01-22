/**************************************************************************************************
	$Id: main.c,v 1.122 2005/12/08 17:45:56 bboy Exp $

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

int	 	shutting_down = 0;		/* Shutdown in progress? */
char		hostname[256];			/* Hostname of local machine */

/**************************************************************************************************
	NAMED_CLEANUP
**************************************************************************************************/
void named_cleanup(int signo) {

  shutting_down = signo;

}

/**************************************************************************************************
	SERVER_STATUS
**************************************************************************************************/
void server_status(void) {
  char			buf[1024], *b = buf;
  time_t		uptime = time(NULL) - Status.start_time;
  unsigned long 	requests = Status.udp_requests + Status.tcp_requests;

  b += snprintf(b, sizeof(buf)-(b-buf), "%s ", hostname);
  b += snprintf(b, sizeof(buf)-(b-buf), "%s %s (%lus) ", _("up"), strsecs(uptime),
		(unsigned long)uptime);
  b += snprintf(b, sizeof(buf)-(b-buf), "%lu %s ", requests, _("questions"));
  b += snprintf(b, sizeof(buf)-(b-buf), "(%.0f/s) ", requests ? AVG(requests, uptime) : 0.0);
  b += snprintf(b, sizeof(buf)-(b-buf), "NOERROR=%u ", Status.results[DNS_RCODE_NOERROR]);
  b += snprintf(b, sizeof(buf)-(b-buf), "SERVFAIL=%u ", Status.results[DNS_RCODE_SERVFAIL]);
  b += snprintf(b, sizeof(buf)-(b-buf), "NXDOMAIN=%u ", Status.results[DNS_RCODE_NXDOMAIN]);
  b += snprintf(b, sizeof(buf)-(b-buf), "NOTIMP=%u ", Status.results[DNS_RCODE_NOTIMP]);
  b += snprintf(b, sizeof(buf)-(b-buf), "REFUSED=%u ", Status.results[DNS_RCODE_REFUSED]);

  /* If the server is getting TCP queries, report on the percentage of TCP queries */
  if (Status.tcp_requests)
    b += snprintf(b, sizeof(buf)-(b-buf), "(%d%% TCP, %lu queries)",
		  (int)PCT(requests, Status.tcp_requests),
		  (unsigned long)Status.tcp_requests);

  Notice("%s", buf);
}
/*--- server_status() ---------------------------------------------------------------------------*/

/**************************************************************************************************
	MYDNS_NAME_2_SHORTNAME
	Removes the origin from a name if it is present.
**************************************************************************************************/
char *mydns_name_2_shortname(char *name, char *origin, int empty_name_is_ok, int notrim) {
  size_t nlen = 0, olen = 0;
  int name_is_dotted, origin_is_dotted;

  if (name) nlen = strlen(name); else return name;
  if (origin) olen = strlen(origin); else return name;

  if (notrim)
    return (name);

  name_is_dotted = (LASTCHAR(name) == '.');
  origin_is_dotted = (LASTCHAR(origin) == '.');

  if (name_is_dotted && !origin_is_dotted) nlen -= 1;
  if (origin_is_dotted && !name_is_dotted) olen -= 1;

  if (nlen < olen)
    return (name);

  if (!strncasecmp(origin, name, nlen)) {
    if (empty_name_is_ok)
      return ("");
    else
      return (name);
  }
  if (!strncasecmp(name + nlen - olen, origin, olen)
      && name[nlen - olen - 1] == '.')
    name[nlen - olen - 1] = '\0';
  return (name);
}
/*--- mydns_name_2_shortname() -----------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
