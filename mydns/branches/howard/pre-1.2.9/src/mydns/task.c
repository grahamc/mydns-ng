/**************************************************************************************************
	$Id: task.c,v 1.86 2006/01/18 20:50:39 bboy Exp $

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
#include "cache.h"
#include "debug.h"
#include "error.h"
#include "recursive.h"
#include "resolve.h"
#include "status.h"
#include "support.h"
#include "taskobj.h"

#include "axfr.h"
#include "buildreply.h"
#include "dnsupdate.h"
#include "ixfr.h"
#include "notify.h"
#include "task.h"
#include "tcp.h"
#include "udp.h"

#if DEBUG_ENABLED
int		debug_task = 0;
#endif

/**************************************************************************************************
	TASK_NEW
	Given a request (TCP or UDP), populates task structure.
	'socktype' should be either SOCK_STREAM or SOCK_DGRAM.
	Returns 0 on success, -1 on error, TASK_ABANDONED if the task is now invalid.
**************************************************************************************************/
taskexec_t
task_new(TASK *t, unsigned char *data, size_t len) {
  unsigned char *qname = NULL, *src = NULL, *qdtop = NULL;
  task_error_t errcode = TASK_FAILED;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_PROGRESS, _("task_new(%p, %p, %u)"), t, data, (unsigned int)len);
#endif

  /* Query needs to at least contain a proper header */
  if (len < DNS_HEADERSIZE) {
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_FORMERR query is missing header"));
#endif
    return formerr(t, DNS_RCODE_FORMERR, ERR_MALFORMED_REQUEST,
		   _("query so small it has no header"));
  }

  /* Refuse queries that are too long */
  if (len > ((t->protocol == SOCK_STREAM) ? DNS_MAXPACKETLEN_TCP : DNS_MAXPACKETLEN_UDP)) {
    Warnx(_("%s: FORMERR in query - too large"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_FORMERR query too large"));
#endif
    return formerr(t, DNS_RCODE_FORMERR, ERR_MALFORMED_REQUEST, _("query too large"));
  }

  /* Parse query header data */
  src = data;
  DNS_GET16(t->id, src);
  memcpy(&t->hdr, src, SIZE16); src += SIZE16;
  DNS_GET16(t->qdcount, src);
  DNS_GET16(t->ancount, src);
  DNS_GET16(t->nscount, src);
  DNS_GET16(t->arcount, src);

  /*
   * Discard queries where the response bit is set;
   * it might be a spoofed packet
   *  asking us to talk to ourselves
   */
  if (t->hdr.qr) {
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_PROGRESS, _("%s: task_new(): %s %s %s"),
	   desctask(t), mydns_rcode_str(DNS_RCODE_FORMERR),
	   err_reason_str(t, ERR_RESPONSE_BIT_SET),
	   _("response bit set on query"));
#endif
    return (TASK_ABANDONED);
  }

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_PROGRESS, _("%s: id=%u qr=%u opcode=%s aa=%u tc=%u rd=%u ra=%u z=%u rcode=%u"), desctask(t),
	 t->id, t->hdr.qr, mydns_opcode_str(t->hdr.opcode),
	 t->hdr.aa, t->hdr.tc, t->hdr.rd, t->hdr.ra, t->hdr.z, t->hdr.rcode);
  Debug(task, DEBUGLEVEL_PROGRESS, _("%s: qd=%u an=%u ns=%u ar=%u"), desctask(t),
	 t->qdcount, t->ancount, t->nscount, t->arcount);
#endif

  task_init_header(t);					/* Initialize header fields for reply */

  t->qdlen = len - DNS_HEADERSIZE;			/* Fill in question data */
  if (t->qdlen <= 0) {
    Warnx(_("%s: FORMERR in query - zero length"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_FORMERR question is zero length"));
#endif
    return formerr(t, DNS_RCODE_FORMERR, ERR_MALFORMED_REQUEST, _("question has zero length"));
  }

  t->qd = ALLOCATE(t->qdlen, uchar*);

  memcpy(t->qd, src, t->qdlen);
  qdtop = src;

  /* Get query name */
  if (!(qname = (unsigned char*)name_unencode((char*)t->qd, t->qdlen, (char**)&src, &errcode))) {
    Warnx(_("%s: FORMERR in query decoding name"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_FORMERR cannot decode name"));
#endif
    return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
  }
  strncpy(t->qname, (char*)qname, sizeof(t->qname)-1);
  RELEASE(qname);

  /* Now we have question data, so initialize encoding */
  if (buildreply_init(t) < 0) {
    Warnx("%s: %s", desctask(t), _("failed to initialize reply"));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns FAILED cannot initialize a reply"));
#endif
    return (TASK_FAILED);
  }

  /* Get query type */
  if (src + SIZE16 > data + len) {
    Warnx(_("%s: FORMERR in query - too short no qtype"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_FORMERR query is too short it is missing the qtype"));
#endif
    return formerr(t, DNS_RCODE_FORMERR, ERR_MALFORMED_REQUEST, _("query too short; no qtype"));
  }

  DNS_GET16(t->qtype, src);

  /* If this request is TCP and TCP is disabled, refuse the request */
  if (t->protocol == SOCK_STREAM && !tcp_enabled && (t->qtype != DNS_QTYPE_AXFR || !axfr_enabled)) {
    Warnx(_("%s: REFUSED query - TCP not enabled"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_REFUSED query TCP and it is not enabled"));
#endif
    return formerr(t, DNS_RCODE_REFUSED, ERR_TCP_NOT_ENABLED, NULL);
  }

  /* Get query class */
  if (src + SIZE16 > data + len) {
    Warnx(_("%s: FORMERR in query - too short no qclass"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_FORMERR query is too short missing the qclass"));
#endif
    return formerr(t, DNS_RCODE_FORMERR, ERR_MALFORMED_REQUEST, _("query too short; no qclass"));
  }

  DNS_GET16(t->qclass, src);

  t->qdlen = src - qdtop;

  /* Request must have at least one question */
  if (!t->qdcount) {
    Warnx(_("%s: FORMERR in query - no questions"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_FORMERR query has no question(s)"));
#endif
    return formerr(t, DNS_RCODE_FORMERR, ERR_NO_QUESTION, _("query contains no questions"));
  }

  /* Server can't handle more than 1 question per packet */
  if (t->qdcount > 1) {
    Warnx(_("%s: FORMERR in query - more than one question"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_FORMERR query has more than one question"));
#endif
    return formerr(t, DNS_RCODE_FORMERR, ERR_MULTI_QUESTIONS,
		   _("query contains more than one question"));
  }

  /* Server won't accept truncated query */
  if (t->hdr.tc) {
    Warnx(_("%s: FORMERR in query - truncated query"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_FORMERR query is truncated"));
#endif
    return formerr(t, DNS_RCODE_FORMERR, ERR_QUESTION_TRUNCATED, _("query is truncated"));
  }

  /*
   * If this is an UPDATE operation, save a copy of the whole query for later parsing
   * If this is a QUERY operation and the query is an IXFR,
   * save a copy of the whole query for later parsing
   */
  if (!t->query) {
    if ((t->hdr.opcode == DNS_OPCODE_UPDATE)
	|| (t->hdr.opcode == DNS_OPCODE_QUERY && t->qtype == DNS_QTYPE_IXFR)) {
      t->len = len;
      t->query = ALLOCATE(t->len, char*);
      memcpy(t->query, data, t->len);
    }
  }

  /* If DNS updates are enabled and the opcode is UPDATE, do the update */
  if (dns_update_enabled && t->hdr.opcode == DNS_OPCODE_UPDATE) {
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns UPDATE result"));
#endif
    return (dnsupdate(t));
  }

  /* Handle Notify messages - currently do nothing so return not implemented */
  if (t->hdr.opcode == DNS_OPCODE_NOTIFY) {
    Warnx(_("%s: FORMERR in query - NOTIFY is currently not a supported opcode"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_NOTIMP query is a NOTIFY"));
#endif
    return formerr(t, DNS_RCODE_NOTIMP, ERR_UNSUPPORTED_OPCODE, NULL);
  }

  /* Server only handles QUERY opcode */
  if (t->hdr.opcode != DNS_OPCODE_QUERY) {
    Warnx(_("%s: FORMERR in query - not a supported opcode"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_NOTIMP query is a QUERY"));
#endif
    return formerr(t, DNS_RCODE_NOTIMP, ERR_UNSUPPORTED_OPCODE, NULL);
  }

  /* Check class (only IN or ANY are allowed unless status is enabled) */
  if ((t->qclass != DNS_CLASS_IN) && (t->qclass != DNS_CLASS_ANY)
#if STATUS_ENABLED
      && (t->qclass != DNS_CLASS_CHAOS)
#endif
      ) {
    Warnx(_("%s: NOTIMP - qclass not available"), desctask(t));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_NOTIMP query is for a qclass we do not support"));
#endif
    return formerr(t, DNS_RCODE_NOTIMP, ERR_NO_CLASS, NULL);
  }

  if (t->qtype == DNS_QTYPE_IXFR && !dns_ixfr_enabled) {
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_REFUSED query is an IXFR and this is switched off"));
#endif
    return formerr(t, DNS_RCODE_REFUSED, ERR_IXFR_NOT_ENABLED, NULL);
  }

  /* If AXFR is requested, it must be TCP, and AXFR must be enabled */
  if (t->qtype == DNS_QTYPE_AXFR && (!axfr_enabled || t->protocol != SOCK_STREAM)) {
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns RCODE_REFUSED query is an AXFR and it is not enabled or not available over the communication channel"));
#endif
    return formerr(t, DNS_RCODE_REFUSED, ERR_NO_AXFR, NULL);
  }

  /* If this is AXFR, fork to handle it so that other requests don't block */
  if (t->protocol == SOCK_STREAM && t->qtype == DNS_QTYPE_AXFR) {
    task_change_type_and_priority(t, IO_TASK, NORMAL_PRIORITY_TASK);
    t->status = NEED_AXFR;
  } else if (t->qtype == DNS_QTYPE_IXFR) {
    t->status = NEED_IXFR;
    task_change_priority(t, NORMAL_PRIORITY_TASK);
  } else {
    t->status = NEED_ANSWER;
  }

#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("task_new returns CONTINUE"));
#endif
  return TASK_CONTINUE;
}
/*--- task_new() --------------------------------------------------------------------------------*/

/**************************************************************************************************
	TASK_INIT_HEADER
	Sets and/or clears header fields and values as necessary.
**************************************************************************************************/
void
task_init_header(TASK *t) {
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_init_header called"), desctask(t));
#endif
  t->hdr.qr = 1;					/* This is the response, not the query */
  t->hdr.ra = forward_recursive;			/* Are recursive queries available? */
  t->hdr.rcode = DNS_RCODE_NOERROR;			/* Assume success unless told otherwise */
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_init_header returns"), desctask(t));
#endif
}
/*--- task_init_header() ------------------------------------------------------------------------*/


/**************************************************************************************************
	TASK_TIMEDOUT
	Check for timed out tasks.
**************************************************************************************************/
taskexec_t
task_timedout(TASK *t) {
  taskexec_t res = TASK_TIMED_OUT;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("task_timedout called"));
#endif
  status_task_timedout(t);

  if (!t) {
    Err(_("task_timedout called with NULL task"));
  }

  if (t->timeextension) {
    res = t->timeextension(t, t->extension);
    if (
	(res == TASK_DID_NOT_EXECUTE)
	|| (res == TASK_EXECUTED)
	|| (res == TASK_CONTINUE)
	) {
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_timedout returns %s"), desctask(t), task_exec_name(res));
#endif
      return res;
    }
  }

  t->reason = ERR_TIMEOUT;
  t->hdr.rcode = DNS_RCODE_SERVFAIL;

  /* Close TCP connection */
  if (t->protocol == SOCK_STREAM)
    sockclose(t->fd);

  dequeue(t);

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("task_timedout returns %s"), task_exec_name(res));
#endif
  return res;
}
/*--- task_timedout() --------------------------------------------------------------------*/

/**************************************************************************************************
	TASK_PROCESS
	Process the specified task, if possible.  Returns a pointer to the next task.
**************************************************************************************************/
static taskexec_t
task_process_query(TASK *t, int rfd, int wfd, int efd) {
  taskexec_t	res = TASK_DID_NOT_EXECUTE;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_PROGRESS, _("%s: task_process_query called rfd = %d, wfd = %d, efd = %d"),
	 desctask(t), rfd, wfd, efd);
#endif

  switch (TASKIOTYPE(t->status)) {

  case Needs2Read:

    switch (t->status) {

    case NEED_READ:
      /*
      **  NEED_READ: Need to read query
      */
      if (rfd || efd) {
	switch (t->protocol) {

	case SOCK_DGRAM:
	  Warnx("%s: %s", desctask(t), _("invalid state for UDP query"));
#if DEBUG_ENABLED
	  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns FAILED"), desctask(t));
#endif
	  return TASK_FAILED;

	case SOCK_STREAM:
	  res = read_tcp_query(t);
	  if (res == TASK_ABANDONED) {
#if DEBUG_ENABLED
	    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns ABANDONED"), desctask(t));
#endif
	    return TASK_ABANDONED;
	  }
	  if (!((res == TASK_FAILED) || (res == TASK_EXECUTED) || (res == TASK_CONTINUE))) {
	    Warnx("%s: %d: %s", desctask(t), (int)res, _("unexpected result from read_tcp_query"));
#if DEBUG_ENABLED
	    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns FAILED"), desctask(t));
#endif
	    return TASK_FAILED;
	  }
#if DEBUG_ENABLED
	  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns %s"), desctask(t), task_exec_name(res));
#endif
	  return res;

	default:
	  Warnx("%s: %d: %s", desctask(t), t->protocol, _("unknown/unsupported protocol"));
#if DEBUG_ENABLED
	  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns FAILED"), desctask(t));
#endif
	  return TASK_FAILED;
	}
      }
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns CONTINUE"), desctask(t));
#endif
      return TASK_CONTINUE;

    case NEED_COMMAND_READ:

      if (!rfd && !efd) {
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns CONTINUE"), desctask(t));
#endif
	return TASK_CONTINUE;
      }

      /* Call the run function */
      if (!t->runextension) {
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns CONTINUE"), desctask(t));
#endif
	return TASK_CONTINUE;
      }

      res = t->runextension(t, t->extension);

#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns %s"), desctask(t), task_exec_name(res));
#endif
      return res;

    default:
      Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns FAILED"), desctask(t));
#endif
      return TASK_FAILED;
    }
    break;

  case Needs2Exec:

    switch (t->status) {

    case NEED_IXFR:

      if (reply_cache_find(t)) {
	char *dest = t->reply;
	DNS_PUT16(dest, t->id);
	DNS_PUT(dest, &t->hdr, SIZE16);
      } else {
	if ((res = ixfr(t, ANSWER, t->qtype, t->qname, 0)) > TASK_FAILED) {
	  buildreply(t, 0);
	  if (t->hdr.tc) {
	    buildreply_abandon(t);
	    if ((res = ixfr(t, ANSWER, t->qtype, t->qname, 1)) <= TASK_FAILED) goto IXFRFAILED;
	    buildreply(t, 0);
	  }
	  if (t->reply_cache_ok) add_reply_to_cache(t);
	}
      IXFRFAILED:
	;
      }
      t->status = NEED_WRITE;
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns CONTINUE"), desctask(t));
#endif
      return TASK_CONTINUE;

    case NEED_ANSWER:
      /*
      **  NEED_ANSWER: Need to resolve query
      */
      if (reply_cache_find(t)) {
	char *dest = t->reply;
	DNS_PUT16(dest, t->id);						/* Query ID */
	DNS_PUT(dest, &t->hdr, SIZE16);					/* Header */
      } else {
	resolve(t, ANSWER, t->qtype, t->qname, 0);
	if (TaskIsRecursive(t->status & Needs2Recurse)) {
#if DEBUG_ENABLED
	  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns CONTINUE"), desctask(t));
#endif
	  return TASK_CONTINUE;
	} else {
	  buildreply(t, 1);
	  if (t->reply_cache_ok)
	    add_reply_to_cache(t);
	}
      }
      t->status = NEED_WRITE;
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns CONTINUE"), desctask(t));
#endif
      return TASK_CONTINUE;

    default:
      Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
      return TASK_FAILED;
    }
    break;

  case Needs2Write:

    switch (t->status) {

    case NEED_WRITE:
      /*
      **  NEED_WRITE: Need to write reply
      */
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_PROGRESS, _("%s: task_process_query processing write - wfd = %d, efd = %d"),
	     desctask(t), wfd, efd);
#endif

      if (wfd || efd) {
	switch (t->protocol) {

	case SOCK_DGRAM:
	  res = write_udp_reply(t);
	  if ((res == TASK_EXECUTED) 
	      || (res == TASK_CONTINUE) 
	      || (res == TASK_FAILED) 
	      || (res == TASK_COMPLETED)) {
#if DEBUG_ENABLED
	    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns %s"), desctask(t), task_exec_name(res));
#endif
	    return res;
	  }
	  Warnx("%s: %d: %s", desctask(t), (int)res,
		_("unexpected result from write_udp_reply"));
#if DEBUG_ENABLED
	  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns FAILED"), desctask(t));
#endif
	  return TASK_FAILED;

	case SOCK_STREAM:
	  res = write_tcp_reply(t);
	  if ((res == TASK_EXECUTED)
	      || (res == TASK_ABANDONED)
	      || (res == TASK_COMPLETED)
	      || (res == TASK_CONTINUE)) {
#if DEBUG_ENABLED
	    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns %s"), desctask(t), task_exec_name(res));
#endif
	    return res;
	  }
	  Warnx("%s: %d: %s", desctask(t), (int)res,
		_("unexpected result from write_tcp_reply"));
#if DEBUG_ENABLED
	  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns FAILED"), desctask(t));
#endif
	  return TASK_FAILED;

	default:
	  Warnx("%s: %d: %s", desctask(t), t->protocol, _("unknown/unsupported protocol"));
#if DEBUG_ENABLED
	  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns FAILED"), desctask(t));
#endif
	  return TASK_FAILED;
	}
      }
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns CONTINUE"), desctask(t));
#endif
      return TASK_CONTINUE;

    case NEED_COMMAND_WRITE:

      if (!wfd && !efd) {
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns CONTINUE"), desctask(t));
#endif
	return TASK_CONTINUE;
      }

      /* Call the run function */
      if (!t->runextension) {
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns CONTINUE"), desctask(t));
#endif
	return TASK_CONTINUE;
      }

      res = t->runextension(t, t->extension);

      return res;

    default:
      Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns FAILED"), desctask(t));
#endif
      return TASK_FAILED;
    }
  }
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_query returns FAILED"), desctask(t));
#endif
  return TASK_FAILED;
}

static taskexec_t
task_process_recursive(TASK *t, int rfd, int wfd, int efd) {
  taskexec_t	res = TASK_DID_NOT_EXECUTE;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_PROGRESS, _("%s: task_process_recursive called rfd = %d, wfd = %d, efd = %d"),
	 desctask(t), rfd, wfd, efd);
#endif

  switch (TASKIOTYPE(t->status)) {

  case Needs2Connect:
	
    switch(t->status) {

    case NEED_RECURSIVE_FWD_CONNECT:
      /*
      **  NEED_RECURSIVE_FWD_CONNECT: Need to connnect to recursive forwarder
      */
      res = recursive_fwd_connect(t);
      if (res == TASK_FAILED) {
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns FAILED"), desctask(t));
#endif
	return TASK_FAILED;
      }
      if (res != TASK_CONTINUE) {
	Warnx("%s: %d: %s", desctask(t), (int)res, _("unexpected result from recursive_fwd_connect"));
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns FAILED"), desctask(t));
#endif
	return TASK_FAILED;
      }
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns CONTINUE"), desctask(t));
#endif
      return TASK_CONTINUE;

    default:
      Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns FAILED"), desctask(t));
#endif
      return TASK_FAILED;
    }
    break;

  case Needs2Write:

    switch (t->status) {

    case NEED_RECURSIVE_FWD_CONNECTING:
      res = recursive_fwd_connecting(t);
      if ((res == TASK_FAILED)
	  || (res == TASK_CONTINUE)) {
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns %s"), desctask(t), task_exec_name(res));
#endif
	return res;
      }
      Warnx("%s: %d: %s", desctask(t), (int)res,
	    _("unexpected result from recursive_fwd_connecting"));
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns FAILED"), desctask(t));
#endif
      return TASK_FAILED;

    case NEED_RECURSIVE_FWD_WRITE:
      /*
      **  NEED_RECURSIVE_FWD_WRITE: Need to write request to recursive forwarder
      */
      res = recursive_fwd_write(t);
      if ((res == TASK_FAILED)
	  || (res == TASK_CONTINUE)) {		/* means try again */
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns %s"), desctask(t), task_exec_name(res));
#endif
	return res;
      }
      Warnx("%s: %d: %s", desctask(t), (int)res, _("unexpected result from recursive_fwd_write"));
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns FAILED"), desctask(t));
#endif
     return TASK_FAILED;

    default:
      Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns FAILED"), desctask(t));
#endif
      return TASK_FAILED;
    }
    break;

  case Needs2Read:

    switch (t->status) {

    case NEED_RECURSIVE_FWD_READ:
      /*
      **  NEED_RECURSIVE_FWD_READ: Need to read reply from recursive forwarder
      */
      if(rfd || efd) {
	res = recursive_fwd_read(t);
	if ((res == TASK_FAILED) 		/* The master task just keeps running */
	    || (res == TASK_COMPLETED)		/* The master task has finished */
	    || (res == TASK_CONTINUE)) {
#if DEBUG_ENABLED
	  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns %s"), desctask(t), task_exec_name(res));
#endif
	  return res;
	}
	Warnx("%s: %d: %s", desctask(t), (int)res, _("unexpected result from recursive_fwd_read"));
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns FAILED"), desctask(t));
#endif
	return TASK_FAILED;
      }
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns CONTINUE"), desctask(t));
#endif
      return TASK_CONTINUE;
	    
    default:
      Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns FAILED"), desctask(t));
#endif
      return TASK_FAILED;;
    }
    break;

  default:

    Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns FAILED"), desctask(t));
#endif
    return TASK_FAILED;
  }
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_recursive returns FAILED"), desctask(t));
#endif
  return TASK_FAILED;
}

static taskexec_t
task_process_request(TASK *t, int rfd, int wfd, int efd) {
  taskexec_t	res = TASK_DID_NOT_EXECUTE;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_PROGRESS, _("%s: task_process_request called rfd = %d, wfd = %d, efd = %d"),
	 desctask(t), rfd, wfd, efd);
#endif

  switch (TASKIOTYPE(t->status)) {

  case Needs2Write:

    switch (t->status) {

    case NEED_NOTIFY_WRITE:
      /*
      ** NEED_NOTIFY_WRITE: need to write to slaves
      */
      if (wfd) {
	res = notify_write(t);
	if ((res == TASK_COMPLETED)
	    || (res == TASK_CONTINUE)) {
#if DEBUG_ENABLED
	  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_request returns %s"), desctask(t), task_exec_name(res));
#endif
	  return res;
	}
	Warnx("%s: %d: %s", desctask(t), (int)res, _("unexpected result from notify_run"));
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_request returns FAILED"), desctask(t));
#endif
	return TASK_FAILED;
      }
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_request returns CONTINUE"), desctask(t));
#endif
      return TASK_CONTINUE;

    default:
      Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_request returns FAILED"), desctask(t));
#endif
      return TASK_FAILED;
    }
    break;

  case Needs2Read:

    switch (t->status) {

    case NEED_NOTIFY_READ:
      /*
      ** NEED_NOTIFY_READ: need to read from a slave
      */
      if (rfd) {
	res = notify_read(t);
	if ((res == TASK_COMPLETED)
	    || (res == TASK_CONTINUE)) {
#if DEBUG_ENABLED
	  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_request returns %s"), desctask(t), task_exec_name(res));
#endif
	  return res;
	}
	Warnx("%s: %d: %s", desctask(t), (int)res, _("unexpected result from notify_run"));
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_request returns FAILED"), desctask(t));
#endif
	return TASK_FAILED;
      }
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_request returns CONTINUE"), desctask(t));
#endif
      return TASK_CONTINUE;

    default:
      Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_request returns FAILED"), desctask(t));
#endif
      return TASK_FAILED;
    }
    break;

  default:
    Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_request returns FAILED"), desctask(t));
#endif
    return TASK_FAILED;
  }
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_request returns FAILED"), desctask(t));
#endif
  return TASK_FAILED;
}

static taskexec_t
task_process_ticktask(TASK *t, int rfd, int wfd, int efd) {

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_PROGRESS, _("%s: task_process_ticktask called rfd = %d, wfd = %d, efd = %d"),
	 desctask(t), rfd, wfd, efd);
#endif

  switch (TASKIOTYPE(t->status)) {

  case Needs2Read:

    Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_ticktask returns FAILED"), desctask(t));
#endif
    return TASK_FAILED;

  case Needs2Write:

    Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_ticktask returns FAILED"), desctask(t));
#endif
    return TASK_FAILED;

  case Needs2Connect:
    Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_ticktask returns FAILED"), desctask(t));
#endif
    return TASK_FAILED;

  case Needs2Exec:
    
    switch (t->status) {

    case NEED_RECURSIVE_FWD_RETRY:
      if (t->timeout <= current_time)
	t->status = NEED_RECURSIVE_FWD_WRITE;
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_ticktask returns CONTINUE"), desctask(t));
#endif
      return TASK_CONTINUE;

    case NEED_NOTIFY_RETRY:
      if (t->timeout <= current_time)
	t->status = NEED_NOTIFY_WRITE;
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_ticktask returns CONTINUE"), desctask(t));
#endif
      return TASK_CONTINUE;

    default:
      Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_ticktask returns FAILED"), desctask(t));
#endif
      return TASK_FAILED;

    }
    break;

  default:
    Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_ticktask returns FAILED"), desctask(t));
#endif
    return TASK_FAILED;

  }
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_ticktask returns FAILED"), desctask(t));
#endif
  return TASK_FAILED;
}

static taskexec_t
task_process_runtask(TASK *t, int rfd, int wfd, int efd) {
  taskexec_t	res = TASK_DID_NOT_EXECUTE;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_PROGRESS, _("%s: task_process_runtask called rfd = %d, wfd = %d, efd = %d"),
	desctask(t), rfd, wfd, efd);
#endif

  switch (t->status) {

  case NEED_AXFR:
    axfr_fork(t);
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_runtask returns COMPLETED"), desctask(t));
#endif
    return TASK_COMPLETED;

  case NEED_TASK_READ:
    if (!rfd && !efd) {
#if DEBUG_ENABLED
      Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_runtask returns CONTINUE"), desctask(t));
#endif
      return TASK_CONTINUE;
    }
    break;

  case NEED_TASK_RUN:
    break;

  default:
    Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_runtask returns FAILED"), desctask(t));
#endif
    return TASK_FAILED;

  }

  /* Call the run function */
  if (!t->runextension) {
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_runtask returns CONTINUE"), desctask(t));
#endif
    return TASK_CONTINUE;
  }

  res = t->runextension(t, t->extension);

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process_runtask returns %s"), desctask(t), task_exec_name(res));
#endif
  return res;
}

int
task_process(TASK *t, int rfd, int wfd, int efd) {
  taskexec_t	res = TASK_DID_NOT_EXECUTE;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_PROGRESS, _("%s: task_process called rfd = %d, wfd = %d, efd = %d"),
	 desctask(t), rfd, wfd, efd);
#endif
  switch (TASKCLASS(t->status)) {

  case QueryTask:

    switch (TaskIsRecursive(t->status)) {

    default: /* Not a recursive query */

      res = task_process_query(t, rfd, wfd, efd);
      break;

    case Needs2Recurse:

      res = task_process_recursive(t, rfd, wfd, efd);
      break;

    }
    break;

  case ReqTask:

    res = task_process_request(t, rfd, wfd, efd);
    break;

  case TickTask:

    res = task_process_ticktask(t, rfd, wfd, efd);
    break;

  case RunTask:

    res = task_process_runtask(t, rfd, wfd, efd);
    break;

  default:
    Warnx("%s: %d %s", desctask(t), t->status, _("unrecognised task status"));
    res = TASK_ABANDONED;
    break;
  }

  switch(res) {

  case TASK_CONTINUE:
    break;

  case TASK_DID_NOT_EXECUTE:
  case TASK_EXECUTED:

    if (current_time > t->timeout)
      if ((res = task_timedout(t)) >= TASK_TIMED_OUT) break;
    goto DEQUEUETASK;
      
  default:
    Warnx("%s: %d %s", desctask(t), res, _("unrecognised task exec state"));
    res = TASK_ABANDONED;

  case TASK_COMPLETED:
  case TASK_FINISHED:
  case TASK_FAILED:
  case TASK_ABANDONED:
  case TASK_TIMED_OUT:

  DEQUEUETASK:

#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_PROGRESS, _("%s: dequeuing task because %s"), desctask(t), task_exec_name(res));
#endif

    if (res == TASK_ABANDONED)
      if (t->protocol == SOCK_STREAM)
	sockclose(t->fd);

    dequeue(t);
    break;
  }

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_process returns %d"), desctask(t), (res != TASK_DID_NOT_EXECUTE));
#endif
  return (res != TASK_DID_NOT_EXECUTE);
}
/*--- task_process() ----------------------------------------------------------------------------*/

void task_free_others(TASK *t, int closeallfds) {
  int i = 0, j = 0;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_PROGRESS, _("%s: Free up all other tasks closeallfds = %d, fd = %d"), desctask(t),
	closeallfds, t->fd);
#endif
  for (i = NORMAL_TASK; i <= PERIODIC_TASK; i++) {
    for (j = HIGH_PRIORITY_TASK; j <= LOW_PRIORITY_TASK; j++) {
      QUEUE *TaskQ = TaskArray[i][j];
      TASK *curtask = (TASK*)(TaskQ->head);
      TASK *nexttask = NULL;
      while (curtask) {
	nexttask = task_next(curtask);
	if (curtask == t) { curtask = nexttask; continue; }
	/* Do not sockclose as the other end(s) are still inuse */
	if (closeallfds && (curtask->protocol != SOCK_STREAM) && curtask->fd >= 0
	    && curtask->fd != t->fd)
	  close(curtask->fd);
	dequeue(curtask);
	curtask = nexttask;
      }
    }
  }
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_free_others returns"), desctask(t));
#endif
}

static struct pollfd *_task_build_IO_item(TASK *t) {
  static struct pollfd	_item;
  struct pollfd		*item = NULL;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_build_IO_item called"), desctask(t));
#endif
  if (t->fd && (t->status & (Needs2Read|Needs2Write))) {
    item = &_item;
    item->fd = t->fd;
    item->events = 0;
    item->revents = 0;
    if (t->status & Needs2Read) {
      item->events |= POLLIN;
    }
    if (t->status & Needs2Write) {
      item->events |= POLLOUT;
    }
  }
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_build_IO_item returns"), desctask(t));
#endif
  return item;
}

static taskexec_t _task_check_timedout(TASK *t, int *timeoutWanted) {
  taskexec_t	res = TASK_DID_NOT_EXECUTE;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_check_timedout called"), desctask(t));
#endif
  if (TASKTIMESOUT(t->status)) {
    time_t	timedOut = t->timeout - current_time;
    if (timedOut <= 0) {
      res = task_timedout(t);
      if ((res == TASK_CONTINUE)
	  || (res == TASK_EXECUTED)
	  || (res == TASK_DID_NOT_EXECUTE)) {
	timedOut = t->timeout - current_time;
	if (timedOut < 0) timedOut = 0;
      } else {
#if DEBUG_ENABLED
	Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_check_timedout returns %s"), desctask(t), task_exec_name(res));
#endif
	return res;
      }
    }
    if (timedOut < *timeoutWanted || *timeoutWanted == -1)
      *timeoutWanted = timedOut;
  }
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: task_check_timedout returns %s"), desctask(t), task_exec_name(res));
#endif
  return res;
}

static void _task_schedule(TASK *t, struct pollfd *items[],
			   int *timeoutWanted, int *numfds, int *maxnumfds) {
  taskexec_t	res = _task_check_timedout(t, timeoutWanted);
  struct pollfd	*item = NULL;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: _task_schedule called"), desctask(t));
#endif
  if ((res != TASK_CONTINUE)
      && (res != TASK_EXECUTED)
      && (res != TASK_DID_NOT_EXECUTE)) {
#if DEBUG_ENABLED
    Debug(task, DEBUGLEVEL_FUNCS, _("%s: _task_schedule returns not ready to run"), desctask(t));
#endif
    return;
  }

  if ((t->status & Needs2Exec) && (t->type != PERIODIC_TASK))
    *timeoutWanted = 0;

  item = _task_build_IO_item(t);

  if (item) {
    int fd = item->fd;
    int i;
    for (i = 0; i < *numfds; i++) {
      if (items
	  && ((&(*items)[i])->fd == fd)) {
	(&(*items)[i])->events |= item->events;
	return;
      }
    }
    *numfds += 1;
    if (*numfds > *maxnumfds) {
      *items = REALLOCATE(*items, *numfds * sizeof(struct pollfd), struct pollfd*);
      *maxnumfds = *numfds;
    }

    memcpy(&((*items)[*numfds-1]), item, sizeof(*item));

  }
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("%s: _task_schedule returns ready to run"), desctask(t));
#endif
  return;
}

static void _task_scheduleQ(QUEUE *taskQ, struct pollfd *items[],
			    int *timeoutWanted, int *numfds, int *maxnumfds) {
  TASK		*t = NULL, *nextTask = NULL;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("_task_scheduleQ called"));
#endif
  for (t = (TASK*)(taskQ->head); t; t = nextTask) {
    nextTask = task_next(t);

    _task_schedule(t, items, timeoutWanted, numfds, maxnumfds);
  }

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("_task_scheduleQ returns"));
#endif
  return;
}

void task_schedule_all(struct pollfd *items[], int *timeoutWanted, int *numfds, int *maxnumfds) {
  int i = 0, j = 0;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("task_schedule_all called"));
#endif
  for (i = NORMAL_TASK; i <= PERIODIC_TASK; i++) {
    for (j = HIGH_PRIORITY_TASK; j <= LOW_PRIORITY_TASK; j++) {
      _task_scheduleQ(TaskArray[i][j], items, timeoutWanted, numfds, maxnumfds);
    }
  }
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("task_schedule_all returns"));
#endif
  return;
}

static void task_purge(TASK *t) {
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("task_purge called"));
#endif
  if (t->protocol != SOCK_STREAM) 
    Notice(_("task_purge() bad task %s => %d"), desctask(t), t->fd);
  dequeue(t);
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("task_purge returns"));
#endif
}

#ifdef PURGING_ENABLED
static void task_purge_all_bad_tasks(void) {
  /* Find out which task has an invalid fd and kill it */
  int i = 0, j = 0;
  TASK *t = NULL, *next_task = NULL;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_PROGRESS, _("task_purge_all_bad_tasks() called"));
#endif

  for (j = HIGH_PRIORITY_TASK; j <= LOW_PRIORITY_TASK; j++) {
    for (i = NORMAL_TASK; i <= PERIODIC_TASK; i++) {
      QUEUE *TaskQ = TaskArray[i][j];
      for (t = (TASK*)(TaskQ->head); t; t = next_task) {
	next_task = task_next(t);
	while (1) {
	  if (t->fd >= 0) {
	    int rv;
	    struct pollfd item;
	    item.fd = t->fd;
	    item.events = POLLIN|POLLOUT|POLLRDHUP;
	    item.revents = 0;
#if HAVE_POLL
	    rv = poll(&item, 1, 0);
#else
#if HAVE_SELECT
	    fd_set rfd, wfd, efd;
	    struct timeval timeout = { 0, 0 };
	    FD_ZERO(&rfd);
	    FD_ZERO(&wfd);
	    FD_ZERO(&efd);
	    FD_SET(t->fd, &rfd);
	    FD_SET(t->fd, &wfd);
	    FD_SET(t->fd, &efd);
	    rv = select(t->fd+1, &rfd, &wfd, &efd, &timeout);
	    if (rv < 0) {
	      if (errno == EBADF) {
		item.revents = POLLNVAL;
		rv = 1;
	      }
	    }
#else
#error You must have either poll(preferred) or select to compile this code
#endif
#endif
	    if (rv < 0) {
	      if (errno == EINTR) { continue; }
	      if (errno == EAGAIN) { /* Could fail here but will log and retry */
		Warn(_("task_purge_all_bad_tasks() received EAGAIN - retrying"));
		continue;
	      }
	      if (errno == EINVAL) {
		Err(_("task_purge_all_bad_tasks() received EINVAL consistency failure in call to poll/select"));
		break;
	      }
	    }
	    if ((item.revents & (POLLERR|POLLHUP|POLLNVAL))) {
	      task_purge(t);
	    }
	  }
	  break;
	}
      }
    }
  }
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_PROGRESS, _("purge_bad_tasks() returned"));
#endif
}
#endif

int task_run_all(struct pollfd items[], int numfds) {
  int i = 0, j = 0, tasks_executed = 0;
  TASK *t = NULL, *next_task = NULL;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("task_run_all called"));
#endif
  /* Process tasks */
  for (j = HIGH_PRIORITY_TASK; j <= LOW_PRIORITY_TASK; j++) {
    for (i = NORMAL_TASK; i <= PERIODIC_TASK; i++) {
      QUEUE *TaskQ = TaskArray[i][j];
      for (t = (TASK*)(TaskQ->head); t; t = next_task) {
	next_task = task_next(t);
	int rfd = 0, wfd = 0, efd = 0;
	int fd = t->fd;
	if (fd >= 0) {
	  int k = 0;
	  struct pollfd *item = NULL;
	  for (k = 0; k < numfds; k++) {
	    if ((&(items[k]))->fd == fd) {
	      item = &(items[k]);
	      rfd |= item->revents & POLLIN;
	      wfd |= item->revents & POLLOUT;
	      efd |= item->revents & (POLLERR|POLLNVAL|POLLHUP);
#if DEBUG_ENABLED
	      Debug(task, DEBUGLEVEL_PROGRESS,
		    _("%s: item fd = %d, events = %x, revents = %x, rfd = %d, wfd = %d, efd = %d"),
		    desctask(t),
		    item->fd, item->events, item->revents,
		    rfd, wfd, efd);
#endif
	      break;
	    }
	  }
	  if (efd) {
	    task_purge(t);
	  } else {
	    if ((t->status & Needs2Read) && !rfd) continue;
	    if ((t->status & Needs2Write) && !wfd) continue;
	  }
#if DEBUG_ENABLED
	  if (!item) {
	    Debug(task, DEBUGLEVEL_PROGRESS, _("%s: No matching item found for fd = %d"), desctask(t), t->fd);
	  }
#endif
	}
	tasks_executed += task_process(t, rfd, wfd, efd);
	if (shutting_down) break;
      }
      if (shutting_down) break;
    }
    if (shutting_down) break;
  }
  task_queue_stats();
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("task_run_all returns"));
#endif
  return tasks_executed;
}


static taskexec_t
check_all_tasks(TASK *mytask, void *mydata) {
  TASK	*t = NULL, *nextt = NULL;
  int	i = 0, j = 0;

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("check_all_tasks called"));
#endif
  /* Reset my timeout so I do not get run again and I do not process myself ;-( */
  mytask->timeout = current_time + task_timeout;

  for (i = NORMAL_TASK; i <= PERIODIC_TASK; i++) {
    for (j = HIGH_PRIORITY_TASK; j <= LOW_PRIORITY_TASK; j++) {
      for (t = (TASK*)(TaskArray[i][j]->head); t; t = nextt) {
	nextt = task_next(t);
	if (current_time > t->timeout)
	  task_timedout(t);
      }
    }
  }

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("check_all_tasks returns"));
#endif
  return (TASK_CONTINUE); /* Run continuously */
}

void
task_start() {

  TASK *inittask = NULL;

  /*
   * Allocate a housekeeping task that will check all of the queues for stuck tasks.
   */

#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("task_start called"));
#endif
  inittask = Ticktask_init(LOW_PRIORITY_TASK, NEED_TASK_RUN, -1, 0, AF_UNSPEC, NULL);
  task_add_extension(inittask, NULL, NULL, NULL, check_all_tasks);
  inittask->timeout = current_time + task_timeout;
#if DEBUG_ENABLED
  Debug(task, DEBUGLEVEL_FUNCS, _("task_start returns"));
#endif
}

/* vi:set ts=3: */
/* NEED_PO */
