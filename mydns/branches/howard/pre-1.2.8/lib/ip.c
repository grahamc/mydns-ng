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
_sockclose(int fd)
{
	if (fd >= 0)
	{
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
char *
ipaddr(int family, void *addr)
{
	static char addrbuf[128];

	addrbuf[0] = '\0';

#if HAVE_IPV6
	if (family == AF_INET6)
		inet_ntop(AF_INET6, addr, addrbuf, sizeof(addrbuf) - 1);
	else
#endif
		inet_ntop(AF_INET, addr, addrbuf, sizeof(addrbuf) - 1);
	return (addrbuf);
}
/*--- ipaddr() ----------------------------------------------------------------------------------*/


#if HAVE_IPV6
/**************************************************************************************************
	IS_IPV6
	Returns 1 if string 's' has two or more ':' characters.
**************************************************************************************************/
int
is_ipv6(char *addr)
{
	register char *c;												/* Current position in 's' */
	register int colons = 0;									/* Number of colons (':') found */

	for (c = addr; *c && (colons < 2); c++)
		if (*c == ':')
			colons++;
	return (colons == 2);
}
/*--- is_ipv6() ---------------------------------------------------------------------------------*/
#endif

/* vi:set ts=3: */
