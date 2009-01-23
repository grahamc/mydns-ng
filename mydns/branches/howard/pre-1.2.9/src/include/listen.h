/**************************************************************************************************
	Copyright (C) 2009-  Howard Wilkinson <howard@cohtech.com>

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

#ifndef _MYDNS_LIB_LISTEN_H
#define _MYDNS_LIB_LISTEN_H

#include "mydnsutil.h"

extern int	*tcp4_fd, *udp4_fd;		/* Listening FD's (IPv4) */
extern int	num_tcp4_fd, num_udp4_fd;	/* Number of listening FD's (IPv4) */
#if HAVE_IPV6
extern int	*tcp6_fd, *udp6_fd;		/* Listening FD's (IPv6) */
extern int	num_tcp6_fd, num_udp6_fd;	/* Number of listening FD's (IPv6) */
#endif

extern void listen_close_all(void);

#endif
