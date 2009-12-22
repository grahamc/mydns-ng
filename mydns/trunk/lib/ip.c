/**************************************************************************************************
	$Id: ip.c,v 1.5 2005/03/22 17:44:56 bboy Exp $

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

#include "mydnsutil.h"



/**************************************************************************************************
	_SOCKCLOSE
	Close/shutdown a socket.
**************************************************************************************************/
void
_sockclose(int fd, const char *file, int line) {
  if (fd >= 0) {
#if DEBUG_ENABLED
    DebugX("enabled", 1, _("sockclose(%d) as requested by %s:%d"), fd, file, line);
#endif
#if HAVE_SHUTDOWN
    shutdown(fd, 2);
#endif
    close(fd);
  }
}
/*--- _sockclose() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	IPADDR
	Returns a textual representation of an IP address.
	'family' should be AF_INET or (if supported) AF_INET6.
	'addr' should be a pointer to a 'struct in_addr' or a 'struct in6_addr'.
**************************************************************************************************/
const char *
ipaddr(int family, void *addr) {
  static char *addrbuf = NULL;
  int addrbufsize = INET_ADDRSTRLEN;
#if HAVE_IPV6
  addrbufsize = MAX(addrbufsize, INET6_ADDRSTRLEN);
#endif
  
  addrbuf = REALLOCATE(addrbuf, addrbufsize, char[]);
  memset(addrbuf, 0, addrbufsize);

  if (family == AF_INET) {
    return(inet_ntop(AF_INET, addr, addrbuf, addrbufsize));
#if HAVE_IPV6
  } else if (family == AF_INET6) {
    return(inet_ntop(AF_INET6, addr, addrbuf, addrbufsize));
#endif
  } else {
    Err(_("Unknown address family"));
  }
  /* NOTREACHED */
  return NULL;
}
/*--- ipaddr() ----------------------------------------------------------------------------------*/


#if HAVE_IPV6
/**************************************************************************************************
	IS_IPV6
	Returns 1 if string 's' has two or more ':' characters.
**************************************************************************************************/
int
is_ipv6(char *addr) {
  register char *c;						/* Current position in 's' */
  register int colons = 0;					/* Number of colons (':') found */

  for (c = addr; *c && (colons < 2); c++)
    if (*c == ':')
      colons++;
  return (colons == 2);
}
/*--- is_ipv6() ---------------------------------------------------------------------------------*/
#endif

/* vi:set ts=3: */
