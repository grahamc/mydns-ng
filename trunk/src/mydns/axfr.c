/**************************************************************************************************
	$Id: axfr.c,v 1.39 2005/05/06 16:06:18 bboy Exp $

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
#define	DEBUG_AXFR	1


#define	AXFR_TIME_LIMIT		3600		/* AXFR may not take more than this long, overall */

static size_t total_records, total_octets;


/**************************************************************************************************
	AXFR_ERROR
	Quits and outputs a warning message.
**************************************************************************************************/
/* Stupid compiler doesn't know exit from _exit... */
/* static void axfr_error(TASK *, const char *, ...) __attribute__ ((__noreturn__)); */
static void
axfr_error(TASK *t, const char *fmt, ...) {
  va_list	ap; 
  char		*msg = NULL;

  if (t) {
    task_output_info(t, NULL);
  } else {
    va_start(ap, fmt);
    VASPRINTF(&msg, fmt, ap);
    va_end(ap);

    Warnx("%s", msg);
    RELEASE(msg);
  }

  sockclose(t->fd);

  _exit(EXIT_FAILURE);
  /* NOTREACHED */
}
/*--- axfr_error() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	AXFR_TIMEOUT
	Hard timeout called by SIGALRM after one hour.
**************************************************************************************************/
static void
axfr_timeout(int dummy) {
  axfr_error(NULL, _("AXFR timed out"));
}
/*--- axfr_timeout() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	AXFR_WRITE_WAIT
	Wait for the client to become ready to read.  Times out after `task_timeout' seconds.
**************************************************************************************************/
static void
axfr_write_wait(TASK *t) {
  int			rv = 0;
  struct pollfd item;
  item.fd = t->fd;
  item.events = POLLOUT;
  item.revents = 0;

#if HAVE_POLL
  rv = poll(&item, 1, -1);
  if (rv >= 0) {
    if (rv != 1 || !(item.revents & POLLOUT) || (item.revents & (POLLERR|POLLHUP|POLLNVAL)))
      axfr_error(t, _("axfr_write_wait write timeout failure"));
  }
#else
#if HAVE_SELECT
  fd_set		wfd;
  fd_set		efd
  struct timeval 	tv = { 0, 0 };

  FD_ZERO(&wfd);
  FD_SET(t->fd, &wfd);
  FD_ZERO(&efd);
  FD_SET(t->fd, &efd);
  tv.tv_sec = task_timeout;
  tv.tv_usec = 0;
  rv = select(t->fd + 1, NULL, &wfd, &efd, &tv);
  if (rv >= 0) {
    if (rv != 1 || !FD_ISSET(t->fd, &wfd) || FD_ISSET(t->fd, &efd))
      axfr_error(t, _("axfr_write_waut write timeout failure"));
  }
#else
#error You must have either poll(preferred) or select to compile this code
#endif
#endif
  if (rv < 0)
    axfr_error(t, "axfr_write_wait poll failed %s(%d)", strerror(errno), errno);
}
/*--- axfr_write_wait() -------------------------------------------------------------------------*/


/**************************************************************************************************
	AXFR_WRITE
	Writes the specified buffer, obeying task_timeout (via axfr_write_wait).
**************************************************************************************************/
static void
axfr_write(TASK *t, char *buf, size_t size) {
  int		rv = 0;
  size_t	offset = 0;

  do {
    axfr_write_wait(t);
    if ((rv = write(t->fd, buf+offset, size-offset)) < 0)
      axfr_error(t, _("write: %s"), strerror(errno));
    if (!rv)
      axfr_error(t, _("client closed connection"));
    offset += rv;
  } while (offset < size);
}
/*--- axfr_write() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	AXFR_REPLY
	Sends one reply to the client.
**************************************************************************************************/
static void
axfr_reply(TASK *t) {
  char len[2] = { 0, 0 }, *l = len;

  build_reply(t, 0);
  DNS_PUT16(l, t->replylen);
  axfr_write(t, len, SIZE16);
  axfr_write(t, t->reply, t->replylen);
  total_octets += SIZE16 + t->replylen;
  total_records++;

  /* Reset the pertinent parts of the task reply data */
  rrlist_free(&t->an);
  rrlist_free(&t->ns);
  rrlist_free(&t->ar);

  RELEASE(t->reply);
  t->replylen = 0;

  name_forget(t);

  RELEASE(t->rdata);
  t->rdlen = 0;

  /* Nuke question data */
  t->qdcount = 0;
  t->qdlen = 0;
}
/*--- axfr_reply() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_XFER
	If the "xfer" column exists in the soa table, it should contain a list of wildcards separated
	by commas.  In order for this zone transfer to continue, one of the wildcards must match
	the client's IP address.
**************************************************************************************************/
static void
check_xfer(TASK *t, MYDNS_SOA *soa) {
  SQL_RES	*res = NULL;
  SQL_ROW	row = NULL;
  char		ip[256];
  char		*query = NULL;
  size_t	querylen = 0;
  int		ok = 0;

  memset(&ip, 0, sizeof(ip));

  if (!mydns_soa_use_xfer)
    return;

  strncpy(ip, clientaddr(t), sizeof(ip)-1);

  querylen = sql_build_query(&query, "SELECT xfer FROM %s WHERE id=%u%s%s%s;",
			     mydns_soa_table_name, soa->id,
			     (mydns_rr_use_active)? " AND active='" : "",
			     (mydns_rr_use_active)? mydns_rr_active_types[0] : "",
			     (mydns_rr_use_active)? "'" : "");

  res = sql_query(sql, query, querylen);
  RELEASE(query);
  if (!res) {
    ErrSQL(sql, "%s: %s", desctask(t), _("error loading zone transfer access rules"));
  }

  if ((row = sql_getrow(res, NULL))) {
    char *wild = NULL, *r = NULL;

    for (r = row[0]; !ok && (wild = strsep(&r, ",")); )	{
      if (strchr(wild, '/')) {
	if (t->family == AF_INET)
	  ok = in_cidr(wild, t->addr4.sin_addr);
      }	else if (wildcard_match(wild, ip))
	ok = 1;
    }
  }
  sql_free(res);

  if (!ok) {
    dnserror(t, DNS_RCODE_REFUSED, ERR_NO_AXFR);
    axfr_reply(t);
    axfr_error(t, _("access denied"));
  }
}
/*--- check_xfer() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	AXFR_ZONE
	DNS-based zone transfer.
**************************************************************************************************/
static void
axfr_zone(TASK *t, MYDNS_SOA *soa) {

  /* Check optional "xfer" column and initialize reply */
  check_xfer(t, soa);
  reply_init(t);

  /* Send opening SOA record */
  rrlist_add(t, ANSWER, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
  axfr_reply(t);

  /*
  **  Get all resource records for zone (if zone ID is nonzero, i.e. not manufactured)
  **  and transmit each resource record.
  */
  if (soa->id) {
    MYDNS_RR *ThisRR = NULL, *rr = NULL;

    if (mydns_rr_load_active(sql, &ThisRR, soa->id, DNS_QTYPE_ANY, NULL, soa->origin) == 0) {
      for (rr = ThisRR; rr; rr = rr->next) {
	/* If 'name' doesn't end with a dot, append the origin */
	if (!*MYDNS_RR_NAME(rr) || LASTCHAR(MYDNS_RR_NAME(rr)) != '.') {
	  mydns_rr_name_append_origin(rr, soa->origin);
	}

#if ALIAS_ENABLED
	/*
	 * If we have been compiled with alias support
	 * and the current record is an alias pass it to alias_recurse()
	 */
	if (rr->alias != 0)
	  alias_recurse(t, ANSWER, MYDNS_RR_NAME(rr), soa, NULL, rr);
	else
#endif
	  rrlist_add(t, ANSWER, DNS_RRTYPE_RR, (void *)rr, MYDNS_RR_NAME(rr));
	/* Transmit this resource record */
	axfr_reply(t);
      }
      mydns_rr_free(ThisRR);
    }
  }

  /* Send closing SOA record */
  rrlist_add(t, ANSWER, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
  axfr_reply(t);

  mydns_soa_free(soa);
}
/*--- axfr_zone() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	AXFR_GET_SOA
	Attempt to find a SOA record.  If SOA id is 0, we made it up.
**************************************************************************************************/
static MYDNS_SOA *
axfr_get_soa(TASK *t) {
  MYDNS_SOA *soa = NULL;

  /* Try to load SOA */
  if (mydns_soa_load(sql, &soa, t->qname) < 0)
    ErrSQL(sql, "%s: %s", desctask(t), _("error loading zone"));
  if (soa) {
    return (soa);
	}

  /* STILL no SOA?  We aren't authoritative */
  dnserror(t, DNS_RCODE_REFUSED, ERR_ZONE_NOT_FOUND);
  axfr_reply(t);
  axfr_error(t, _("unknown zone"));
  /* NOTREACHED */
  return (NULL);
}
/*--- axfr_get_soa() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	AXFR
	DNS-based zone transfer.  Send all resource records for in QNAME's zone to the client.
**************************************************************************************************/
void
axfr(TASK *t) {
#if DEBUG_ENABLED && DEBUG_AXFR
  struct timeval start = { 0, 0}, finish = { 0, 0 };	/* Time AXFR began and ended */
#endif
  MYDNS_SOA *soa = NULL;				/* SOA record for zone (may be bogus!) */

  /* Do generic startup stuff; this is a child process */
  signal(SIGALRM, axfr_timeout);
  alarm(AXFR_TIME_LIMIT);
  sql_close(sql);
  db_connect();

#if DEBUG_ENABLED && DEBUG_AXFR
  gettimeofday(&start, NULL);
  DebugX("axfr", 1,_("%s: Starting AXFR for task ID %u"), desctask(t), t->internal_id);
#endif
  total_records = total_octets = 0;
  t->no_markers = 1;

  /* Get SOA for zone */
  soa = axfr_get_soa(t);

  if (soa){
    /* Transfer that zone */
    axfr_zone(t, soa);
  }

#if DEBUG_ENABLED && DEBUG_AXFR
  /* Report result */
  gettimeofday(&finish, NULL);
  DebugX("axfr", 1,_("AXFR: %u records, %u octets, %.3fs"), 
	 (unsigned int)total_records, (unsigned int)total_octets,
	 ((finish.tv_sec + finish.tv_usec / 1000000.0) - (start.tv_sec + start.tv_usec / 1000000.0)));
#endif
  t->qdcount = 1;
  t->an.size = total_records;
  task_output_info(t, NULL);

  sockclose(t->fd);

  _exit(EXIT_SUCCESS);
}
/*--- axfr() ------------------------------------------------------------------------------------*/

void
axfr_fork(TASK *t) {
  int pfd[2] = { -1, -1 };				/* Parent/child pipe descriptors */
  pid_t pid = -1, parent = -1;

#if DEBUG_ENABLED && DEBUG_AXFR 
  DebugX("axfr", 1,_("%s: axfr_fork called on fd %d"), desctask(t), t->fd);
#endif

  if (pipe(pfd))
    Err(_("pipe"));
  parent = getpid();
  if ((pid = fork()) < 0) {
    close(pfd[0]);
    close(pfd[1]);
    Warn(_("%s: fork"), clientaddr(t));
    return;
  }

  if (!pid) {
    /* Child: reset all signal handlers to default before we dive off elsewhere */
    struct sigaction act;

    memset(&act, 0, sizeof(act));

    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = SIG_DFL;

    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGUSR2, &act, NULL);
    sigaction(SIGALRM, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

#if DEBUG_ENABLED && DEBUG_AXFR
    DebugX("axfr", 1,_("%s: axfr_fork is in the child"), desctask(t));
#endif

    /*  Let parent know I have started */
    close(pfd[0]);
    if (write(pfd[1], "OK", 2) != 2)
      Warn(_("error writing startup notification"));
    close(pfd[1]);

#if DEBUG_ENABLED && DEBUG_AXFR
    DebugX("axfr", 1,_("%s: axfr_fork child has told parent I am running"), desctask(t));
#endif

    /* Clean up parents resources */
    free_other_tasks(t, 1);

#if DEBUG_ENABLED && DEBUG_AXFR
    DebugX("axfr", 1,_("%s: AXFR child built"), desctask(t));
#endif
    /* Do AXFR */
    axfr(t);
  } else {	/* Parent */
    char	buf[5] = "\0\0\0\0\0";
    int		errct = 0;

    close(pfd[1]);

    for (errct = 0; errct < 5; errct++) {
      if (read(pfd[0], &buf, 4) != 2)
	Warn(_("%s (%d of 5)"), _("error reading startup notification"), errct+1);
      else
	break;
    }
    close(pfd[0]);

#if DEBUG_ENABLED && DEBUG_AXFR
    DebugX("axfr", 1,_("AXFR: process started on pid %d for TCP fd %d, task ID %u"), pid, t->fd, t->internal_id);
#endif
  }
  /* NOTREACHED*/
}

/* vi:set ts=3: */
/* NEED_PO */
