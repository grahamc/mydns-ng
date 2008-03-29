/**************************************************************************************************
	$Id: cidr.c,v 1.6 2005/04/20 16:49:11 bboy Exp $

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
	IN_CIDR
	Checks to see if the specified IP address is within the specified CIDR range.
	XXX: Fix to work with IPv6
	Returns 1 if so, 0 if not.
**************************************************************************************************/
int
in_cidr(char *cidr, struct in_addr ip) {
  unsigned int tmp[8];
  struct in_addr network, netmask;

  network.s_addr = 0;
  netmask.s_addr = 0;

  if (sscanf(cidr, "%u.%u.%u.%u/%u.%u.%u.%u",
	     &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5], &tmp[6], &tmp[7]) == 8) {
    network.s_addr = htonl((tmp[0] << 24) + (tmp[1] << 16) + (tmp[2] << 8) + tmp[3]);
    netmask.s_addr = htonl((tmp[4] << 24) + (tmp[5] << 16) + (tmp[6] << 8) + tmp[7]);
  } else if (sscanf(cidr, "%u.%u.%u.%u/%u", &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4]) == 5) {
    network.s_addr = htonl((tmp[0] << 24) + (tmp[1] << 16) + (tmp[2] << 8) + tmp[3]);
    while (tmp[4] > 0) {
      netmask.s_addr = (netmask.s_addr >> 1) + 0x80000000;
      tmp[4]--;
    }
    netmask.s_addr = htonl(netmask.s_addr);
  } else return (0);

  return ((ip.s_addr & netmask.s_addr) == (network.s_addr & netmask.s_addr));
}
/*--- in_cidr() ---------------------------------------------------------------------------------*/

/* vi:set ts=3: */
