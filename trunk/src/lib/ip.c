/**************************************************************************************************
	$Id: ip.c,v 1.9 2005/04/20 16:43:22 bboy Exp $
	ip.c: Routines to manipulate IP addresses.

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
	MYDNS_REVSTR_IP4
	Provided with a string like "4.3.2.1", returns the numeric equivalent (of "1.2.3.4") as an
	IPv4 address, in host byte order.
**************************************************************************************************/
uint32_t mydns_revstr_ip4(const uchar *s) {
  register const uchar	*c;
  register int		n;
  uint8_t		ip[4];
  uint32_t		rv;

  ip[0] = ip[1] = ip[2] = ip[3] = 0;
  for (c = s, n = 0; *c && n < 4; c++)
    if (c == s || *c == '.') {
      if (*c == '.') c++;
      ip[3-n] = (uint8_t)atoi((char*)c);
      n++;
    }
  memcpy(&rv, ip, sizeof(rv));
  return (rv);
}
/*--- mydns_revstr_ip4() ------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_EXTRACT_ARPA
	Given an "in-addr.arpa" address, loads the specified octets and returns the number of octets
	found.
**************************************************************************************************/
int mydns_extract_arpa(const uchar *s, uint8_t ip[]) {
  register const uchar	*c;
  register int	n;

  ip[0] = ip[1] = ip[2] = ip[3] = 0;
  for (c = s, n = 0; *c && n < 4; c++)
    if (c == s || *c == '.') {
      if (*c == '.') c++;
      if (!isdigit(*c))
	return (n);
      ip[n++] = (uint8_t)atoi((char*)c);
    }
  return (n);
}
/*--- mydns_extract_arpa() ----------------------------------------------------------------------*/

/* vi:set ts=3: */
