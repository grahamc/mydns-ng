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

/* Make this nonzero to enable debugging for this source file */
#define	DEBUG_LIB_TASK	1

#define MAXTASKS		(USHRT_MAX + 1)
#define TASKVECSZ		(MAXTASKS/BITSPERBYTE)

#define TASKVEC_ZERO(TV)	memset((void*)(TV), 0, TASKVECSZ)
#define TASKVEC_CLR(TI, TV)	FD_CLR((TI), (fd_set*)(TV))
#define TASKVEC_SET(TI, TV)	FD_SET((TI), (fd_set*)(TV))
#define TASKVEC_ISSET(TI, TV)	FD_ISSET((TI), (fd_set*)(TV))

static uint8_t		*taskvec = NULL;
static uint16_t		internal_id = 0;

uint32_t 	answer_then_quit = 0;		/* Answer this many queries then quit */

QUEUE 		*TaskArray[PERIODIC_TASK+1][LOW_PRIORITY_TASK+1];

TASK *task_find_by_id(TASK *t, QUEUE *TaskQ, unsigned long id) {
  TASK	*ThisT = NULL;

  for (ThisT = (TASK*)TaskQ->head; ThisT ; ThisT = task_next(ThisT)) {
    if (ThisT->internal_id == id) return ThisT;
  }
#if DEBUG_ENABLED && DEBUG_LIB_TASK
  DebugX("lib-task", 1, _("%s: task_find_by_id(%s, %ld) cannot find task on queue"),
	 desctask(t), TaskQ->queuename, id);
#endif
  return NULL;
}

char * task_exec_name(taskexec_t rv) {
  switch(rv) {

  case TASK_ABANDONED:			return _("Task Abandoned");
  case TASK_FAILED:			return _("Task Failed");

  case TASK_COMPLETED:			return _("Task Completed");
  case TASK_FINISHED:			return _("Task Finished");
  case TASK_TIMED_OUT:			return _("Task Timed Out");

  case TASK_EXECUTED:			return _("Task Executed");
  case TASK_DID_NOT_EXECUTE:		return _("Task did not execute");
  case TASK_CONTINUE:			return _("Task Continue");

  default:
    {
      static char *msg = NULL;
      if (msg) RELEASE(msg);
      ASPRINTF(&msg, _("Task Exec Code %d"), rv);
      return msg;
    }
  }
  /*NOTREACHED*/
}

char * task_type_name(int type) {
  switch (type) {

  case NORMAL_TASK:			return _("Normal Task");
  case IO_TASK:				return _("IO Driven Task");
  case PERIODIC_TASK:			return _("Clock Driven Task");

  default:
    {
      static char *msg = NULL;
      ASPRINTF(&msg, _("Task Type %d"), type);
      return msg;
    }
  }
  /*NOTREACHED*/
}

char * task_priority_name(int priority) {
  switch (priority) {

  case HIGH_PRIORITY_TASK:		return _("High Priority");
  case NORMAL_PRIORITY_TASK:		return _("Normal Priority");
  case LOW_PRIORITY_TASK:		return _("Low Priority");

  default:
    {
      static char *msg = NULL;
      ASPRINTF(&msg, _("Task Priority %d"), priority);
      return msg;
    }
  }
  /*NOTREACHED*/
}

char * task_status_name(TASK *t) {

  switch (t->status) {

  case NEED_READ:			return _("NEED_READ");
  case NEED_IXFR:			return _("NEED_IXFR");
  case NEED_ANSWER:			return _("NEED_ANSWER");
  case NEED_WRITE:			return _("NEED_WRITE");

  case NEED_RECURSIVE_FWD_CONNECT:	return _("NEED_RECURSIVE_FWD_CONNECT");
  case NEED_RECURSIVE_FWD_CONNECTING:	return _("NEED_RECURSIVE_FWD_CONNECTING");
  case NEED_RECURSIVE_FWD_WRITE:	return _("NEED_RECURSIVE_FWD_WRITE");
  case NEED_RECURSIVE_FWD_RETRY:	return _("NEED_RECURSIVE_FWD_RETRY");
  case NEED_RECURSIVE_FWD_READ:		return _("NEED_RECURSIVE_FWD_READ");

  case NEED_NOTIFY_READ:		return _("NEED_NOTIFY_READ");
  case NEED_NOTIFY_WRITE:		return _("NEED_NOTIFY_WRITE");
  case NEED_NOTIFY_RETRY:		return _("NEED_NOTIFY_RETRY");

  case NEED_TASK_RUN:			return _("NEED_TASK_RUN");
  case NEED_AXFR:			return _("NEED_AXFR");
  case NEED_TASK_READ:			return _("NEED_TASK_READ");

  case NEED_COMMAND_READ:		return _("NEED_COMMAND_READ");
  case NEED_COMMAND_WRITE:		return _("NEED_COMMAND_WRITE");

  default:
    {
      static char *msg = NULL;
      ASPRINTF(&msg, _("Task Status %X"), t->status);
      return msg;
    }
  }
  /*NOTREACHED*/
}

/**************************************************************************************************
	CLIENTADDR
	Given a task, returns the client's IP address in printable format.
**************************************************************************************************/
const char * clientaddr(TASK *t) {
  void *addr = NULL;
  const char *res = NULL;

  if (t->family == AF_INET) {
    addr = &t->addr4.sin_addr;
#if HAVE_IPV6
  } else if (t->family == AF_INET6) {
    addr = &t->addr6.sin6_addr;
#endif
  } else {
    return(_("Address unknown"));
  }

  res = ipaddr(t->family, addr);
  return res;
}
/*--- clientaddr() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	DESCTASK
	Describe a task; used by error/warning/debug output.
**************************************************************************************************/
char * desctask(TASK *t) {
  static char *desc = NULL;

  if (desc) RELEASE(desc);

  ASPRINTF(&desc, "%s: %s %s (%u) %s, %s %s",
	   clientaddr(t), mydns_rr_get_type_by_id(t->qtype)->rr_type_name,
	   t->qname ? (char *)t->qname : _("<NONE>"),
	   t->internal_id, task_status_name(t), task_priority_name(t->priority),
	   task_type_name(t->type)) < 0;

  return (desc);
}
/*--- desctask() --------------------------------------------------------------------------------*/


/**************************************************************************************************
	_TASK_FREE
	Free the memory used by a task.
**************************************************************************************************/
void
_task_free(TASK *t, const char *file, int line) {

  if (!t) return;

#if DEBUG_ENABLED && DEBUG_TASK
  DebugX("lib-task", 1, _("%s: Freeing task at %s:%d"), desctask(t), file, line);
#endif

  if (t->protocol == SOCK_STREAM && t->fd >= 0) {
    close(t->fd);
  }

  if (t->extension && t->freeextension) {
    t->freeextension(t, t->extension);
  }
	
  RELEASE(t->extension);
	  
#if DYNAMIC_NAMES
  {
    register int n;
    for (n = 0; n < t->numNames; n++)
      RELEASE(t->Names[n]);
    if (t->numNames)
      RELEASE(t->Names);
    RELEASE(t->Offsets);
  }
#endif

  RELEASE(t->query);
  RELEASE(t->qd);
  rrlist_free(&t->an);
  rrlist_free(&t->ns);
  rrlist_free(&t->ar);
  RELEASE(t->rdata);
  RELEASE(t->reply);

  TASKVEC_CLR(t->id, taskvec);

  RELEASE(t);

  if (answer_then_quit && (Status.udp_requests + Status.tcp_requests) >= answer_then_quit)
    named_cleanup(SIGQUIT);
}
/*--- _task_free() ------------------------------------------------------------------------------*/

void
task_add_extension(TASK *t, void *extension, FreeExtension freeextension, RunExtension runextension,
		   TimeExtension timeextension)
{
  if (t->extension && t->freeextension) {
    t->freeextension(t, t->extension);
  }
  RELEASE(t->extension);

  t->extension = extension;
  t->freeextension = freeextension;
  t->runextension = runextension;
  t->timeextension = timeextension;
}

void
task_remove_extension(TASK *t) {
  if (t->extension && t->freeextension) {
    t->freeextension(t, t->extension);
  }
  RELEASE(t->extension);
  t->extension = NULL;
  t->freeextension = NULL;
  t->runextension = NULL;
  t->timeextension = NULL;
  return;
}

/**************************************************************************************************
	_TASK_INIT
	Allocates and initializes a new task, and returns a pointer to it.
	t = task_init(taskpriority, NEED_ZONE, fd, SOCK_DGRAM, AF_INET, &addr);
**************************************************************************************************/
TASK *
_task_init(
	   tasktype_t		type,			/* Type of task to create */
	   taskpriority_t	priority,		/* Priority of queue to use */
	   taskstat_t		status,			/* Initial status */
	   int 			fd,			/* Associated file descriptor for socket */
	   int			protocol,		/* Protocol (SOCK_DGRAM or SOCK_STREAM) */
	   int 			family,			/* Protocol (SOCK_DGRAM or SOCK_STREAM) */
	   void 		*addr,			/* Remote address */
	   const char		*file,
	   int			line
) {
  TASK				*new = NULL;
  QUEUE				**TaskQ = NULL;
  uint16_t			id = 0;

  if (!taskvec) {
    taskvec = (uint8_t*)ALLOCATE(TASKVECSZ, uint8_t[]);
    TASKVEC_ZERO(taskvec);
  }

  new = ALLOCATE(sizeof(TASK), TASK);

  new->status = status;
  new->fd = fd;
  new->protocol = protocol;
  new->family = family;
  if (addr) {
    if (family == AF_INET) {
      memcpy(&new->addr4, addr, sizeof(struct sockaddr_in));
#if HAVE_IPV6
    } else if (family == AF_INET6) {
      memcpy(&new->addr6, addr, sizeof(struct sockaddr_in6));
#endif
    }
  }
  new->type = type;
  new->priority = priority;
  while(TASKVEC_ISSET(id = internal_id++, taskvec)) continue;
  TASKVEC_SET(id, taskvec);
  new->internal_id = id;
  new->timeout = current_time + task_timeout;
  new->minimum_ttl = DNS_MINIMUM_TTL;
  new->reply_cache_ok = 1;

  new->extension = NULL;
  new->freeextension = NULL;
  new->runextension = NULL;
  new->timeextension = NULL;

  TaskQ = &(TaskArray[type][priority]);

  if (enqueue(TaskQ, new) < 0) {
    task_free(new);
    return (NULL);
  }

  return (new);
}
/*--- _task_init() -------------------------------------------------------------------------------*/

void
_task_change_type(TASK *t, tasktype_t type, taskpriority_t priority) {
  requeue(&TaskArray[type][priority], t);
  t->type = type;
  t->priority = priority;
}

/**************************************************************************************************
	TASK_OUTPUT_INFO
**************************************************************************************************/
void
task_output_info(TASK *t, char *update_desc) {
#if !DISABLE_DATE_LOGGING
  struct timeval tv = { 0, 0 };
  time_t tt = 0;
  struct tm *tm = NULL;
  char datebuf[80]; /* This is magic and needs rethinking - string should be ~ 23 characters */
#endif

  /* If we've already outputted the info for this (i.e. multiple DNS UPDATE requests), ignore */
  if (t->info_already_out)
    return;

  /* Don't output anything for TCP sockets in the process of closing */
  if (t->protocol == SOCK_STREAM && t->fd < 0)
    return;

#if !DISABLE_DATE_LOGGING
  gettimeofday(&tv, NULL);
  tt = tv.tv_sec;
  tm = localtime(&tt);

  strftime(datebuf, sizeof(datebuf)-1, "%d-%b-%Y %H:%M:%S", tm);
#endif

  Verbose(
#if !DISABLE_DATE_LOGGING
	  "%s+%06lu "
#endif
	  "#%u "
	  "%u "		/* Client-provided ID */
	  "%s "		/* TCP or UDP? */
	  "%s "		/* Client IP */
	  "%s "		/* Class */
	  "%s "		/* Query type (A, MX, etc) */
	  "%s "		/* Name */
	  "%s "		/* Return code (NOERROR, NXDOMAIN, etc) */
	  "%s "		/* Reason */
	  "%u "		/* Question section */
	  "%u "		/* Answer section */
	  "%u "		/* Authority section */
	  "%u "		/* Additional section */
	  "LOG "
	  "%s "		/* Reply from cache? */
	  "%s "		/* Opcode */
	  "\"%s\""	/* UPDATE description (if any) */
	  ,
#if !DISABLE_DATE_LOGGING
	  datebuf, tv.tv_usec,
#endif
	  t->internal_id,
	  t->id,
	  (t->protocol == SOCK_STREAM)?"TCP"
	  :(t->protocol == SOCK_DGRAM)?"UDP"
	  :"UNKNOWN",
	  clientaddr(t),
	  mydns_class_str(t->qclass),
	  mydns_rr_get_type_by_id(t->qtype)->rr_type_name,
	  t->qname,
	  mydns_rcode_str(t->hdr.rcode),
	  err_reason_str(t, t->reason),
	  (unsigned int)t->qdcount,
	  (unsigned int)t->an.size,
	  (unsigned int)t->ns.size,
	  (unsigned int)t->ar.size,
	  (t->reply_from_cache ? "Y" : "N"),
	  mydns_opcode_str(t->hdr.opcode),
	  update_desc ? update_desc : ""
	  );
}
/*--- task_output_info() ------------------------------------------------------------------------*/

/**************************************************************************************************
	_TASK_ENQUEUE
	Enqueues a TASK item, appending it to the end of the list.
**************************************************************************************************/
int _task_enqueue(QUEUE **q, TASK *t, const char *file, unsigned int line) {

  queue_append(q, t);

  t->len = 0;							/* Reset TCP packet len */

  if (t->protocol == SOCK_STREAM)
    Status.tcp_requests++;
  else if (t->protocol == SOCK_DGRAM)
    Status.udp_requests++;

#if DEBUG_ENABLED && DEBUG_TASK
  DebugX("lib-task", 1,_("%s: enqueued (by %s:%u)"), desctask(t), file, line);
#endif

  return (0);
}
/*--- _enqueue() --------------------------------------------------------------------------------*/

/**************************************************************************************************
	_TASK_DEQUEUE
	Removes the item specified from the queue.  Pass this a pointer to the actual element in the
	queue.
	For `error' pass 0 if the task was dequeued due to sucess, 1 if dequeued due to error.
**************************************************************************************************/
void _task_dequeue(QUEUE **q, TASK *t, const char *file, unsigned int line) {
#if DEBUG_ENABLED && DEBUG_QUEUE
  char *taskdesc = STRDUP(desctask(t));

  DebugX("lib-task", 1,_("%s: dequeuing (by %s:%u)"), taskdesc, file, line);
#endif

  if (err_verbose)				/* Output task info if being verbose */
    task_output_info(t, NULL);

  if (t->hdr.rcode >= 0 && t->hdr.rcode < MAX_RESULTS)		/* Store results in stats */
    Status.results[t->hdr.rcode]++;

  queue_remove(q, t);

  task_free(t);
#if DEBUG_ENABLED && DEBUG_QUEUE
  DebugX("lib-task", 1,_("%s: dequeued (by %s:%u)"), taskdesc, file, line);
  RELEASE(taskdesc);
#endif
}
/*--- _dequeue() --------------------------------------------------------------------------------*/

void _task_requeue(QUEUE **q, TASK *t, const char *file, unsigned int line) {
#if DEBUG_ENABLED && DEBUG_QUEUE
  char *taskdesc = desctask(t);
  DebugX("lib-task", 1,_("%s: requeuing (by %s:%u) called"), taskdesc, file, line);
#endif

  queue_remove(task_queue(t), t);
  queue_append(q, t);

}

static void _task_1_queue_stats(QUEUE *q) {
#if DEBUG_ENABLED && DEBUG_QUEUE
  char		*msg = NULL;
  int		msgsize = 512;
  int		msglen = 0;
  QueueEntry	*t = NULL;

#if !DISABLE_DATE_LOGGING
  struct timeval tv = { 0, 0 };
  time_t tt = 0;
  struct tm *tm = NULL;
  char datebuf[80]; /* This is magic and needs rethinking - string should be ~ 23 characters */

  gettimeofday(&tv, NULL);
  tt = tv.tv_sec;
  tm = localtime(&tt);

  strftime(datebuf, sizeof(datebuf)-1, "%d-%b-%Y %H:%M:%S", tm);
#endif

  DebugX("queue", 1,_(
#if !DISABLE_DATE_LOGGING
		      "%s+%06lu "
#endif
		      "%s size=%u, max size=%u"),
#if !DISABLE_DATE_LOGGING
	 datebuf, tv.tv_usec,
#endif
	 q->queuename, (unsigned int)q->size, (unsigned int)q->max_size);
	  
  msg = ALLOCATE(msgsize, char[]);

  msg[0] = '\0';
  for (t = q->head; t; t = t->next) {
    int idsize;
    idsize = snprintf(&msg[msglen], msgsize - msglen, " %u", t->internal_id);
    msglen += idsize;
    if ((msglen + 2*idsize) >= msgsize) msg = REALLOCATE(msg, msgsize *= 2, char[]);
  }
  if (msglen)
    DebugX("queue", 1,_("Queued tasks %s"), msg);

  RELEASE(msg);
#endif
}

void task_queue_stats() {
  int i = 0, j = 0;

  for (i = NORMAL_TASK; i <= PERIODIC_TASK; i++) {
    for (j= HIGH_PRIORITY_TASK; j <= LOW_PRIORITY_TASK; j++) {
      _task_1_queue_stats(TaskArray[j][i]);
    }
  }
}

/* vi:set ts=3: */
/* NEED_PO */
