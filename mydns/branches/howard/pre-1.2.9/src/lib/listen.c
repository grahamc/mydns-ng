/**************************************************************************************************

	Copyright (C) 2009- Howard Wilkinson <howard@cohtech.com>

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

#include "listen.h"

#if DEBUG_ENABLED
int		debug_listen = 0;
#endif

int *udp4_fd = (int *)NULL;					/* Listening socket: UDP, IPv4 */
int *tcp4_fd = (int *)NULL;					/* Listening socket: TCP, IPv4 */
int num_udp4_fd = 0;						/* Number of items in 'udp4_fd' */
int num_tcp4_fd = 0;						/* Number of items in 'tcp4_fd' */

#if HAVE_IPV6
int *udp6_fd = (int *)NULL;					/* Listening socket: UDP, IPv6 */
int *tcp6_fd = (int *)NULL;					/* Listening socket: TCP, IPv6 */
int num_udp6_fd = 0;						/* Number of items in 'udp6_fd' */
int num_tcp6_fd = 0;						/* Number of items in 'tcp6_fd' */
#endif

void listen_close_all(void) {
  int n;

  /* Close listening FDs - do not sockclose these are shared with other processes */
  for (n = 0; n < num_tcp4_fd; n++)
    close(tcp4_fd[n]);

  for (n = 0; n < num_udp4_fd; n++)
    close(udp4_fd[n]);

#if HAVE_IPV6
  for (n = 0; n < num_tcp6_fd; n++)
    close(tcp6_fd[n]);

  for (n = 0; n < num_udp6_fd; n++)
    close(udp6_fd[n]);
#endif	/* HAVE_IPV6 */

}

