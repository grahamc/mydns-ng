/**************************************************************************************************
	$Id: tcp.c,v 1.47 2005/04/20 17:30:38 bboy Exp $

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

#include "bits.h"
#include "debug.h"
#include "listen.h"
#include "support.h"
#include "taskobj.h"

#include "task.h"
#include "tcp.h"

#if DEBUG_ENABLED
int		debug_tcp = 0;
#endif

/**************************************************************************************************
	ACCEPT_TCP_QUERY
**************************************************************************************************/
int
accept_tcp_query(int fd, int family) {
  struct sockaddr_in	addr4;
#if HAVE_IPV6
  struct sockaddr_in6	addr6;
#endif
  struct sockaddr	*addr = NULL;
  socklen_t		addrlen = 0;
  int			rmt_fd  = -1;
  TASK			*t = NULL;

#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_FUNCS, _("accept_tcp_query called"));
#endif
  memset(&addr4, 0, sizeof(addr4));
#if HAVE_IPV6
  memset(&addr6, 0, sizeof(addr6));
#endif

  if (family == AF_INET) {
    addrlen = sizeof(addr4);
    addr = (struct sockaddr*)&addr4;
#if HAVE_IPV6
  } else if (family == AF_INET6) {
    addrlen = sizeof(addr6);
    addr = (struct sockaddr*)&addr6;
#endif
  } else {
    Err(_("accept_tcp_query: cannot accept fd %d family %d not known"),
	fd,
	family);
  }

  if ((rmt_fd = accept(fd, addr, &addrlen)) < 0) {
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
#if DEBUG_ENABLED
      Debug(tcp, DEBUGLEVEL_FUNCS, _("accept_tcp_query returns -1 would block"));
#endif
      return (-1);
    }
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("accept_tcp_query returns -1 accept failed"));
#endif
    return Warn(_("accept_tcp_query: accept failed on fd %d proto %s"),
		fd,
		(family == AF_INET) ? "IPV4"
#if HAVE_IPV6
		: (family == AF_INET6) ? "IPV6"
#endif
		: _("UNKNOWN"));
  }
  fcntl(rmt_fd, F_SETFL, fcntl(rmt_fd, F_GETFL, 0) | O_NONBLOCK);
  if (!(t = IOtask_init(HIGH_PRIORITY_TASK, NEED_READ, rmt_fd, SOCK_STREAM, family, addr))) {
    sockclose(rmt_fd);
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("accept_tcp_query returns -1 could not initialize task"));
#endif
    return (-1);
  }

  t->len = 0;

#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_FUNCS, _("accept_tcp_query returns %d"), rmt_fd);
#endif
  return rmt_fd;
}
/*--- accept_tcp_query() ------------------------------------------------------------------------*/


/**************************************************************************************************
	READ_TCP_LENGTH
	The first two octets of a TCP question are the length.  Read them.
	Returns > 0 on success, -1 on failure. 0 on try again later
**************************************************************************************************/
static taskexec_t
read_tcp_length(TASK *t) {
  int	rv = 0;
  char	len[2] = { 0, 0 };

#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_length called"), desctask(t));
#endif
  if ((rv = recv(t->fd, len, 2, 0)) != 2) {
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
#if DEBUG_ENABLED
	Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_length returns CONTINUE would block"), desctask(t));
#endif
	return (TASK_CONTINUE);
      }
      if (errno != ECONNRESET)
	Warn(_("read_tcp_length: receive failed for fd %d: %s"),
	     t->fd,
	     clientaddr(t));
    } else if (rv == 0) {
      /* Client connection closed */
      Notice(_("read_tcp_length: no data for fd %d: %s: %s"),
	     t->fd,
	     clientaddr(t),
	     _("Client closed TCP connection"));
    } else {
      Warnx(_("read_tcp_length: incorrect data on fd %d: %s: %s"),
	    t->fd,
	    clientaddr(t),
	    _("TCP message length invalid"));
    }
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_length returns ABANDONED"), desctask(t));
#endif
    return (TASK_ABANDONED);
  }

  if ((t->len = ((len[0] << 8) | (len[1]))) < DNS_HEADERSIZE) {
    Warnx(_("read_tcp_length: read on fd %d: %s: %s (%u octet%s)"),
	  t->fd,
	  clientaddr(t),
	  _("TCP message too short"),
	  (unsigned int)t->len,
	  S(t->len));
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_length returns ABANDONED message too short"), desctask(t));
#endif
    return (TASK_ABANDONED);
  }
  if (t->len > DNS_MAXPACKETLEN_TCP) {
    Warnx(_("read_tcp_length: read on fd %d: %s: %s (%u octet%s)"),
	  t->fd,
	  clientaddr(t),
	  _("TCP message too long"),
	  (unsigned int)t->len,
	  S(t->len));
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_length returns ABANDONED message too long"), desctask(t));
#endif
    return (TASK_ABANDONED);
  }

  t->query = ALLOCATE(t->len + 1, char*);

  t->offset = 0;

#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_length returns COMPLETED"), desctask(t));
#endif
  return (TASK_COMPLETED);
}
/*--- read_tcp_length() -------------------------------------------------------------------------*/


/**************************************************************************************************
	READ_TCP_QUERY
	Returns >= 0 on success, -1 on failure.
**************************************************************************************************/
int
read_tcp_query(TASK *t) {
  unsigned char	*end = NULL;
  int		rv = 0;
  taskexec_t	res = TASK_FAILED;

#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_query called"), desctask(t));
#endif
  /* Read packet length if we haven't already */
  if (!t->len) {
    res = read_tcp_length(t);
    if ((res == TASK_CONTINUE)
	|| (res == TASK_ABANDONED)) {
#if DEBUG_ENABLED
      Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_query returns %s"), desctask(t), task_exec_name(res));
#endif
      return res;
    }
    if (res != TASK_COMPLETED) {
      Warnx(_("read_tcp_query: %s: %d %s"),
	      desctask(t),
	    (int)res,
	    _("unexpected result from read_tcp_length"));
#if DEBUG_ENABLED
      Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_query returns ABANDONED"), desctask(t));
#endif
      return (TASK_ABANDONED);
    }
  }

  end = (unsigned char*)(t->query + t->len);

  /* Read whatever data is ready */
  if ((rv = recv(t->fd, t->query + t->offset, t->len - t->offset, 0)) < 0) {
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
#if DEBUG_ENABLED
      Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_query returns CONTINUE would block"), desctask(t));
#endif
      return (TASK_CONTINUE);
    }
    Warn(_("read_tcp_query: receive on fd %d: %s: %s %s"),
	 t->fd,
	 clientaddr(t),
	 _("recv (TCP)"),
	 strerror(errno));
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_query returns ABANDONED"), desctask(t));
#endif
    return (TASK_ABANDONED);
  }
  if (rv == 0) {
    Notice(_("read_tcp_query: receive on fd %d: %s: %s"),
	   t->fd,
	   clientaddr(t),
	   _("Client closed TCP connection"));
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_query returns ABANDONED client closed connection"), desctask(t));
#endif
    return (TASK_ABANDONED);				/* Client closed connection */
  }

#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_PROGRESS, _("%s: 2+%d TCP octets in"), clientaddr(t), rv);
#endif

  t->offset += rv;
  if (t->offset > t->len) {
    Warnx(_("read_tcp_query: receive on fd %d: %s: %s"),
	  t->fd,
	  clientaddr(t),
	  _("TCP message data too long"));
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_query returns ABANDONED message dtat too long"), desctask(t));
#endif
    return (TASK_ABANDONED);
  } else if (t->offset < t->len) {
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_query returns EXECUTED need more datat"), desctask(t));
#endif
    return (TASK_EXECUTED);				/* Not finished reading */
  }
  t->offset = 0;					/* Reset offset for writing reply */
  
#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: read_tcp_query returns new task"), desctask(t));
#endif
  return task_new(t, (unsigned char*)t->query, t->len);
}
/*--- read_tcp_query() --------------------------------------------------------------------------*/


/**************************************************************************************************
	WRITE_TCP_LENGTH
	Writes the length octets for TCP reply.  Returns 0 on success, -1 on failure.
**************************************************************************************************/
static taskexec_t
write_tcp_length(TASK *t)
{
  char	len[2] = { 0, 0 };
  char	*l = NULL;
  int	rv = 0;

#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_length called"), desctask(t));
#endif
  l = len;
  DNS_PUT16(l, t->replylen);

  if ((rv = write(t->fd, len + t->offset, SIZE16 - t->offset)) < 0) {
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
#if DEBUG_ENABLED
      Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_length returns CONTINUE would block"), desctask(t));
#endif
      return (TASK_CONTINUE); /* Try again later */
    }
    Warn(_("write_tcp_length: write on fd %d: %s: %s"),
	 t->fd,
	 clientaddr(t),
	 _("write (length) (TCP)"));
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_length returns ABANDONED"), desctask(t));
#endif
    return (TASK_ABANDONED);
  }
  if (rv == 0) {
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_length returns ABANDONED client closed connection"), desctask(t));
#endif
    return (TASK_ABANDONED);		/* Client closed connection */
  }
  t->offset += rv;
  if (t->offset >= SIZE16) {
    t->len_written = 1;
    t->offset = 0;
  }
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_length returns COMPLETED"), desctask(t));
#endif
  return (TASK_COMPLETED);
}
/*--- write_tcp_length() ------------------------------------------------------------------------*/


/**************************************************************************************************
	WRITE_TCP_REPLY
	Returns 0 on success, -1 on error.  If -1 is returned, the task is no longer valid.
**************************************************************************************************/
taskexec_t
write_tcp_reply(TASK *t)
{
  int 			rv = 0, rmt_fd = -1;
  taskexec_t		res = TASK_FAILED;
  struct sockaddr_in	addr4;
#if HAVE_IPV6
  struct sockaddr_in6	addr6;
#endif
  struct sockaddr	*addr = NULL, *addrsave = NULL;
  int			addrlen = 0;
  TASK			*newt = NULL;

#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_reply called"), desctask(t));
#endif
  memset(&addr4, 0, sizeof(addr4));
#if HAVE_IPV6
  memset(&addr6, 0, sizeof(addr6));
#endif

  /* Write TCP length if we haven't already */
  if (!t->len_written) {
    if ((res = write_tcp_length(t)) < TASK_COMPLETED)	{
#if DEBUG_ENABLED
      Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_reply returns %s"), desctask(t), task_exec_name(res));
#endif
      return (res);
    }
    if (res == TASK_CONTINUE) { /* Connection blocked try again later */
#if DEBUG_ENABLED
      Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_reply returns CONTINUE blocked"), desctask(t));
#endif
      return(TASK_CONTINUE);
    }
    if (!t->len_written) {
#if DEBUG_ENABLED
      Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_reply returns CONTINUE partial write"), desctask(t));
#endif
     return (TASK_CONTINUE);
    }
  }

  /* Write the reply */
  if ((rv = write(t->fd, t->reply + t->offset, t->replylen - t->offset)) < 0) {
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
#if DEBUG_ENABLED
      Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_reply returns CONTINUE would block"), desctask(t));
#endif
      return (TASK_CONTINUE);
    }
    Warn(_("write_tcp_reply: write on fd %d: %s: %s"),
	 t->fd,
	 clientaddr(t),
	 _("write (TCP)"));
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_reply returns ABANDONED"), desctask(t));
#endif
    return (TASK_ABANDONED);
  }
  if (rv == 0) {
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_reply returns ABANDONED client closed connection"), desctask(t));
#endif
    /* Client closed connection */
    return (TASK_ABANDONED);
  }
  t->offset += rv;
  if (t->offset < t->replylen) {
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_reply returns CONTINUE partial write"), desctask(t));
#endif
    return (TASK_CONTINUE);	/* Not finished yet... */
  }
  
  /* Task complete; reset.  The TCP client must be able to perform multiple queries on
     the same connection (BIND8 AXFR does this for sure) */
  if (t->family == AF_INET) {
    addr = (struct sockaddr*)&t->addr4;
    addrsave = (struct sockaddr*)&addr4;
    addrlen = sizeof(struct sockaddr_in);
#if HAVE_IPV6
  } else if (t->family == AF_INET6) {
    addr = (struct sockaddr*)&t->addr6;
    addrsave = (struct sockaddr*)&addr6;
    addrlen = sizeof(struct sockaddr_in6);
#endif
  }
  
  memcpy(addrsave, addr, addrlen);
  rmt_fd = t->fd;

  /* Reinitialize to allow multiple queries on TCP */
  if (!(newt = IOtask_init(t->priority, NEED_READ, rmt_fd, SOCK_STREAM, t->family, addrsave))) {
#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_reply returns ABANDONED failed to create new task"), desctask(t));
#endif
    return (TASK_ABANDONED);
  }

  t->fd = -1;

#if DEBUG_ENABLED
    Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: write_tcp_reply returns COMPLETED"), desctask(t));
#endif
  return (TASK_COMPLETED);
}
/*--- write_tcp_reply() -------------------------------------------------------------------------*/

static taskexec_t
tcp_tick(TASK *t, void *data) {
#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: tcp_tick called"), desctask(t));
#endif
  t->timeout = current_time + task_timeout;

#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: tcp_tick returns CONTINUE"), desctask(t));
#endif
  return TASK_CONTINUE;
}

static taskexec_t
tcp_read_message(TASK *t, void *data) {
  int		newfd = -1;

#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: tcp_read_message called"), desctask(t));
#endif
  while ((newfd = accept_tcp_query(t->fd, t->family)) >= 0) continue;

  t->timeout = current_time + task_timeout;

#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_FUNCS, _("%s: tcp_read_message returns CONTINUE"), desctask(t));
#endif
  return TASK_CONTINUE;
}

void
tcp_start() {
  int		n = 0;

#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_FUNCS, _("tcp_start called"));
#endif
  for (n = 0; n < num_tcp4_fd; n++) {
    TASK *tcptask = IOtask_init(HIGH_PRIORITY_TASK, NEED_TASK_READ,
				tcp4_fd[n], SOCK_STREAM, AF_INET, NULL);
    task_add_extension(tcptask, NULL, NULL, tcp_read_message, tcp_tick);
  }
#if HAVE_IPV6
  for (n = 0; n < num_tcp6_fd; n++) {
    TASK *tcptask = IOtask_init(HIGH_PRIORITY_TASK, NEED_TASK_READ,
				tcp6_fd[n], SOCK_STREAM, AF_INET6, NULL);
    task_add_extension(tcptask, NULL, NULL, tcp_read_message, tcp_tick);
  }
#endif
#if DEBUG_ENABLED
  Debug(tcp, DEBUGLEVEL_FUNCS, _("tcp_start returns"));
#endif
}
/* vi:set ts=3: */
/* NEED_PO */
