/**************************************************************************************************
	$Id: udp.c,v 1.41 2005/04/20 16:49:12 bboy Exp $

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

/* Make this nonzero to enable debugging for this source file */
#define	DEBUG_UDP	1

extern int	*udp4_fd;			/* Listening FD's (IPv4) */
extern int	num_udp4_fd;			/* Number of listening FD's (IPv4) */
#if HAVE_IPV6
extern int	*udp6_fd;			/* Listening FD's (IPv6) */
extern int	num_udp6_fd;			/* Number of listening FD's (IPv6) */
#endif

/**************************************************************************************************
	READ_UDP_QUERY
	Returns 0 on success (a task was added), -1 on failure.
**************************************************************************************************/
taskexec_t
read_udp_query(int fd, int family) {
  struct sockaddr	addr;
  char			in[DNS_MAXPACKETLEN_UDP];
  socklen_t 		addrlen = 0;
  int			len = 0;
  TASK			*t = NULL;
  taskexec_t		rv = TASK_FAILED;

  memset(&addr, 0, sizeof(addr));
  memset(&in, 0, sizeof(in));
    
  /* Read message */
  if (family == AF_INET) {
    addrlen = sizeof(struct sockaddr_in);
#if HAVE_IPV6
  } else if (family == AF_INET6) {
    addrlen = sizeof(struct sockaddr_in6);
#endif
  }

  if ((len = recvfrom(fd, &in, sizeof(in), 0, &addr, &addrlen)) < 0) {
    if (
	(errno == EINTR)
#ifdef EAGAIN
	|| (errno == EAGAIN)
#else
#ifdef EWOULDBLOCK
	|| (errno == EWOULDBLOCK)
#endif
#endif
	) {
      return (TASK_CONTINUE);
    }
    return Warn("%s", _("recvfrom (UDP)"));
  }
  if (len == 0) {
    return (TASK_FAILED);
  }
  if (!(t = IOtask_init(HIGH_PRIORITY_TASK, NEED_ANSWER, fd, SOCK_DGRAM, family, &addr)))
    return (TASK_FAILED);

#if DEBUG_ENABLED && DEBUG_UDP
  DebugX("udp", 1, "%s: %d %s", clientaddr(t), len, _("UDP octets in"));
#endif
  rv = task_new(t, (unsigned char*)in, len);
  if (rv < TASK_FAILED) {
    dequeue(t);
    rv = TASK_FAILED;
  }
  return rv;
}
/*--- read_udp_query() --------------------------------------------------------------------------*/


/**************************************************************************************************
	WRITE_UDP_REPLY
**************************************************************************************************/
taskexec_t
write_udp_reply(TASK *t) {
  int			rv = 0;
  struct sockaddr	*addr = NULL;
  int			addrlen = 0;

  if (t->family == AF_INET) {
    addr = (struct sockaddr*)&t->addr4;
    addrlen = sizeof(struct sockaddr_in);
#if HAVE_IPV6
  } else if (t->family == AF_INET6) {
    addr = (struct sockaddr*)&t->addr6;
    addrlen = sizeof(struct sockaddr_in6);
#endif
  }
	
  rv = sendto(t->fd, t->reply, t->replylen, 0, addr, addrlen);

  if (rv < 0) {
    if (
	(errno == EINTR)
#ifdef EAGAIN
	|| (errno == EAGAIN)
#else
#ifdef EWOULDBLOCK
	|| (errno == EWOULDBLOCK)
#endif
#endif
	) {
      return (TASK_CONTINUE); /* Try again */
    }
    if (errno != EPERM && errno != EINVAL)
      Warn("%s: %s", desctask(t), _("sendto (UDP)"));
    return (TASK_FAILED);
  }

  if (rv == 0) {
    /*
     * Should never happen as this implies the "other" end has closed the socket
     * and we do not have another end - this is UDP
     * However, we get this return when a route to the client does not exist
     * this happens over WAN connection and VPN connections that flap
     * so try again and see if the connection returns - this is going to
     * make the process run continuously ....
     */
    return (TASK_EXECUTED);
    /* Err("%s: Send to (UDP) returned 0", desctask(t)); */
  }

  if (rv != (int)t->replylen) {
    /*
     * This should never ever happen as we have sent a partial packet over UDP
     */
    Err(_("%s: Send to (UDP) returned %d when writing %u"), desctask(t), rv, (unsigned int)t->replylen);
  }

#if DEBUG_ENABLED && DEBUG_UDP
  DebugX("udp", 1, _("%s: WRITE %u UDP octets (id %u)"), desctask(t), (unsigned int)t->replylen, t->id);
#endif
  return (TASK_COMPLETED);
}
/*--- write_udp_reply() -------------------------------------------------------------------------*/
static taskexec_t
udp_tick(TASK *t, void *data) {

  t->timeout = current_time + task_timeout;

  return TASK_CONTINUE;
}

static taskexec_t
udp_read_message(TASK *t, void *data) {
  taskexec_t	res;

  while((res = read_udp_query(t->fd, t->family)) != TASK_CONTINUE) continue;

  t->timeout = current_time + task_timeout;

  return TASK_CONTINUE;
}

void
udp_start() {
  int		n = 0;

  for (n = 0; n < num_udp4_fd; n++) {
    TASK *udptask = IOtask_init(HIGH_PRIORITY_TASK, NEED_TASK_READ,
				udp4_fd[n], SOCK_DGRAM, AF_INET, NULL);
    task_add_extension(udptask, NULL, NULL, udp_read_message, udp_tick);
  }
#if HAVE_IPV6
  for (n = 0; n < num_udp6_fd; n++) {
    TASK *udptask = IOtask_init(HIGH_PRIORITY_TASK, NEED_TASK_READ,
				udp6_fd[n], SOCK_DGRAM, AF_INET6, NULL);
    task_add_extension(udptask, NULL, NULL, udp_read_message, udp_tick);
  }
#endif
}

/* vi:set ts=3: */
/* NEED_PO */
