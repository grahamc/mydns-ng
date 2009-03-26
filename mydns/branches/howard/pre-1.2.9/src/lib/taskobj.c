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
#include "debug.h"
#include "error.h"
#include "rr.h"
#include "rrtype.h"
#include "status.h"
#include "support.h"
#include "taskobj.h"

#if DEBUG_ENABLED
int		debug_taskobj = 0;
#endif

#define MAXTASKS		(USHRT_MAX + 1)
#define TASKVECSZ		(MAXTASKS/BITSPERBYTE)

#define TASKVEC_ZERO(TV)	memset((void*)(TV), 0, TASKVECSZ)

static uint32_t		taskvec_masks[] = {
  0x1,					/* 2^0 */
  0x2,					/* 2^1 */
  0x4,					/* 2^2 */
  0x8,					/* 2^3 */
  0x10,					/* 2^4 */
  0x20,					/* 2^5 */
  0x40,					/* 2^6 */
  0x80,					/* 2^7 */
  0x100,				/* 2^8 */
  0x200,				/* 2^9 */
  0x400,				/* 2^10 */
  0x800,				/* 2^11 */
  0x1000,				/* 2^12 */
  0x2000,				/* 2^13 */
  0x4000,				/* 2^14 */
  0x8000,				/* 2^15 */
  0x10000,				/* 2^16 */
  0x20000,				/* 2^17 */
  0x40000,				/* 2^18 */
  0x80000,				/* 2^19 */
  0x100000,				/* 2^20 */
  0x200000,				/* 2^21 */
  0x400000,				/* 2^22 */
  0x800000,				/* 2^23 */
  0x1000000,				/* 2^24 */
  0x2000000,				/* 2^25 */
  0x4000000,				/* 2^26 */
  0x8000000,				/* 2^27 */
  0x10000000,				/* 2^28 */
  0x20000000,				/* 2^29 */
  0x40000000,				/* 2^30 */
  0x80000000				/* 2^31 */
};

static uint32_t		*taskvec = NULL;
static uint32_t		internal_id = 0;
static int32_t		active_tasks = 0;

uint32_t 	answer_then_quit = 0;		/* Answer this many queries then quit */

QUEUE 		*TaskArray[PERIODIC_TASK+1][LOW_PRIORITY_TASK+1];

TASK *task_find_by_id(TASK *t, QUEUE *TaskQ, unsigned long id) {
  TASK	*ThisT = NULL;

  for (ThisT = (TASK*)TaskQ->head; ThisT ; ThisT = task_next(ThisT)) {
    if (ThisT->internal_id == id) return ThisT;
  }
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_PROGRESS, _("%s: task_find_by_id(%s, %ld) cannot find task on queue"),
	desctask(t), TaskQ->queuename, id);
#endif
  return NULL;
}

const char *task_exec_name(taskexec_t rv) {
  const char *buf;

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_exec_name called"));
#endif
  switch(rv) {

  case TASK_ABANDONED:			buf = _("Task Abandoned"); break;
  case TASK_FAILED:			buf = _("Task Failed"); break;

  case TASK_COMPLETED:			buf = _("Task Completed"); break;
  case TASK_FINISHED:			buf = _("Task Finished"); break;
  case TASK_TIMED_OUT:			buf = _("Task Timed Out"); break;

  case TASK_EXECUTED:			buf = _("Task Executed"); break;
  case TASK_DID_NOT_EXECUTE:		buf = _("Task did not execute"); break;
  case TASK_CONTINUE:			buf = _("Task Continue"); break;

  default:
    {
      static char *msg = NULL;
      RELEASE(msg);
      ASPRINTF(&msg, _("Task Exec Code %d"), rv);
      buf = (const char*)msg;
      break;
    }
  }

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_exec_name returns %s"), buf);
#endif
  return buf;
}

const char * task_type_name(int type) {
  const char *buf = NULL;

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_type_name called"));
#endif
  switch (type) {

  case NORMAL_TASK:			buf = _("Normal Task"); break;
  case IO_TASK:				buf = _("IO Driven Task"); break;
  case PERIODIC_TASK:			buf = _("Clock Driven Task"); break;

  default:
    {
      static char *msg = NULL;
      RELEASE(msg);
      ASPRINTF(&msg, _("Task Type %d"), type);
      buf = (const char*)msg;
    }
    break;
  }

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_type_name returns %s"), buf);
#endif
  return buf;
}

const char * task_priority_name(int priority) {
  const char *buf = NULL;

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_priority_name called"));
#endif
  switch (priority) {

  case HIGH_PRIORITY_TASK:		buf = _("High Priority"); break;
  case NORMAL_PRIORITY_TASK:		buf = _("Normal Priority"); break;
  case LOW_PRIORITY_TASK:		buf = _("Low Priority"); break;

  default:
    {
      static char *msg = NULL;
      RELEASE(msg);
      ASPRINTF(&msg, _("Task Priority %d"), priority);
      buf = (const char*)msg;
    }
 break;
  }

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_priority_name returns %s"), buf);
#endif
  return buf;
}

const char * task_status_name(TASK *t) {
  const char *buf = NULL;

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_status_name called"));
#endif
  switch (t->status) {

  case NEED_READ:			buf = _("NEED_READ"); break;
  case NEED_IXFR:			buf = _("NEED_IXFR"); break;
  case NEED_ANSWER:			buf = _("NEED_ANSWER"); break;
  case NEED_WRITE:			buf = _("NEED_WRITE"); break;

  case NEED_RECURSIVE_FWD_CONNECT:	buf = _("NEED_RECURSIVE_FWD_CONNECT"); break;
  case NEED_RECURSIVE_FWD_CONNECTING:	buf = _("NEED_RECURSIVE_FWD_CONNECTING"); break;
  case NEED_RECURSIVE_FWD_WRITE:	buf = _("NEED_RECURSIVE_FWD_WRITE"); break;
  case NEED_RECURSIVE_FWD_RETRY:	buf = _("NEED_RECURSIVE_FWD_RETRY"); break;
  case NEED_RECURSIVE_FWD_READ:		buf = _("NEED_RECURSIVE_FWD_READ"); break;
  case NEED_RECURSIVE_FWD_CONNECTED:	buf = _("NEED_RECURSIVE_FWD_CONNECTED"); break;

  case NEED_NOTIFY_READ:		buf = _("NEED_NOTIFY_READ"); break;
  case NEED_NOTIFY_WRITE:		buf = _("NEED_NOTIFY_WRITE"); break;
  case NEED_NOTIFY_RETRY:		buf = _("NEED_NOTIFY_RETRY"); break;

  case NEED_TASK_RUN:			buf = _("NEED_TASK_RUN"); break;
  case NEED_AXFR:			buf = _("NEED_AXFR"); break;
  case NEED_TASK_READ:			buf = _("NEED_TASK_READ"); break;

  case NEED_COMMAND_READ:		buf = _("NEED_COMMAND_READ"); break;
  case NEED_COMMAND_WRITE:		buf = _("NEED_COMMAND_WRITE"); break;

  default:
    {
      static char *msg = NULL;
      ASPRINTF(&msg, _("Task Status %X"), t->status);
      buf = (const char*)msg;
    }
    break;
  }

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_status_name returns %s"), buf);
#endif
  return buf;
}

/**************************************************************************************************
	CLIENTADDR
	Given a task, returns the client's IP address in printable format.
**************************************************************************************************/
const char * clientaddr(TASK *t) {
  void *addr = NULL;
  const char *res = NULL;

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("clientaddr called"));
#endif
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
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("clientaddr returns %s"), res);
#endif
  return res;
}
/*--- clientaddr() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	DESCTASK
	Describe a task; used by error/warning/debug output.
**************************************************************************************************/
char * desctask(TASK *t) {
  static char *desc = NULL;

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("desctask called"));
#endif
  if (desc) RELEASE(desc);

  ASPRINTF(&desc, "%s: %s %s (%u) %s, %s %s",
	   clientaddr(t), mydns_rr_get_type_by_id(t->qtype)->rr_type_name,
	   t->qname ? (char *)t->qname : _("<NONE>"),
	   t->internal_id, task_status_name(t), task_priority_name(t->priority),
	   task_type_name(t->type));

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("desctask returns"));
#endif
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

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_PROGRESS, _("%s: Freeing task with fd = %d at %s:%d"),
	desctask(t), t->fd, file, line);
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

  taskvec[t->internal_id >> 5] &= ~taskvec_masks[t->internal_id & 0x1ff];

  RELEASE(t);

  if (--active_tasks < 0) {
    Err(_("Less than zero tasks running ..."));
  }

  if (answer_then_quit && (status_udp_requests() + status_tcp_requests()) >= answer_then_quit)
    named_cleanup(SIGQUIT);
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("_task_free returns"));
#endif
}
/*--- _task_free() ------------------------------------------------------------------------------*/

void
task_add_extension(TASK *t, void *extension, FreeExtension freeextension, RunExtension runextension,
		   TimeExtension timeextension)
{
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_add_extension called"));
#endif
  if (t->extension && t->freeextension) {
    t->freeextension(t, t->extension);
  }
  RELEASE(t->extension);

  t->extension = extension;
  t->freeextension = freeextension;
  t->runextension = runextension;
  t->timeextension = timeextension;
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_add_extension returns"));
#endif
}

void
task_remove_extension(TASK *t) {
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_remove_extension called"));
#endif
  if (t->extension && t->freeextension) {
    t->freeextension(t, t->extension);
  }
  RELEASE(t->extension);
  t->extension = NULL;
  t->freeextension = NULL;
  t->runextension = NULL;
  t->timeextension = NULL;
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_remove_extension returns"));
#endif
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
  int				taskvec_index;
  uint32_t			taskvec_mask;
  int				wrap_round = 0;

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("_task_init called"));
#endif
  if (active_tasks++ >= MAXTASKS) {
    active_tasks -= 1;
    Notice(_("More than %d tasks running can't service this task"), MAXTASKS);
#if DEBUG_ENABLED
    Debug(taskobj, DEBUGLEVEL_FUNCS, _("_task_init returns NULL too many tasks queued in this process"));
#endif
    return NULL;
  }
  
  if (!taskvec) {
    taskvec = (uint32_t*)ALLOCATE(TASKVECSZ, uint32_t*);
    TASKVEC_ZERO(taskvec);
  }

  while (1) {
    id = internal_id++;
    if (internal_id >= MAXTASKS) {
      if (wrap_round) {
	Notice(_("internal_id wrapped around twice while trying to find an empty slot"));
#if DEBUG_ENABLED
	Debug(taskobj, DEBUGLEVEL_FUNCS, _("_task_init returns NULL cannot find a free task slot in this process"));
#endif
	return NULL;
      }
      internal_id = 0;
      wrap_round = 1;
    }
    taskvec_index = id >> 5;
    taskvec_mask = taskvec_masks[id & 0x1ff];
    if (!taskvec[taskvec_index]
	|| (taskvec[taskvec_index] & taskvec_mask) == 0) break;
  }
  taskvec[taskvec_index] |= taskvec_mask;

  new = ALLOCATE(sizeof(TASK), TASK*);

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
#if DEBUG_ENABLED
    Debug(taskobj, DEBUGLEVEL_FUNCS, _("_task_init returns NULL could not queue new task in this process"));
#endif
    return (NULL);
  }

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("_task_init returns new task %s"), desctask(new));
#endif
  return (new);
}
/*--- _task_init() -------------------------------------------------------------------------------*/

void
_task_change_type(TASK *t, tasktype_t type, taskpriority_t priority) {
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("_task_change_type called"));
#endif
  requeue(&TaskArray[type][priority], t);
  t->type = type;
  t->priority = priority;
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("_task_change_type returns"));
#endif
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

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_output_info called"));
#endif
  /* If we've already outputted the info for this (i.e. multiple DNS UPDATE requests), ignore */
  if (t->info_already_out) {
#if DEBUG_ENABLED
    Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_output_info returns already output information"));
#endif
    return;
  }

  /* Don't output anything for TCP sockets in the process of closing */
  if (t->protocol == SOCK_STREAM && t->fd < 0) {
#if DEBUG_ENABLED
    Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_output_info returns tcp socket closing"));
#endif
    return;
  }

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
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_output_info returns having output information"));
#endif
}
/*--- task_output_info() ------------------------------------------------------------------------*/

/**************************************************************************************************
	_TASK_ENQUEUE
	Enqueues a TASK item, appending it to the end of the list.
**************************************************************************************************/
int _task_enqueue(QUEUE **q, TASK *t, const char *file, unsigned int line) {

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("_task_enqueue called from %s:%u"), file, line);
#endif
  queue_append(q, t);

  t->len = 0;							/* Reset TCP packet len */

  if (t->protocol == SOCK_STREAM)
    status_tcp_request(t);
  else if (t->protocol == SOCK_DGRAM)
    status_udp_request(t);

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_PROGRESS, _("%s: enqueued (by %s:%u)"), desctask(t), file, line);
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
#if DEBUG_ENABLED
  char *taskdesc = STRDUP(desctask(t));

  Debug(taskobj, DEBUGLEVEL_PROGRESS, _("%s: dequeuing (by %s:%u)"), taskdesc, file, line);
#endif

  if (err_verbose)				/* Output task info if being verbose */
    task_output_info(t, NULL);

  status_result(t, t->hdr.rcode);

  queue_remove(q, t);

  task_free(t);
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_PROGRESS, _("%s: dequeued (by %s:%u)"), taskdesc, file, line);
  RELEASE(taskdesc);
#endif
}
/*--- _dequeue() --------------------------------------------------------------------------------*/

void _task_requeue(QUEUE **q, TASK *t, const char *file, unsigned int line) {
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_PROGRESS, _("%s: requeuing (by %s:%u) called"), desctask(t), file, line);
#endif

  queue_remove(task_queue(t), t);
  queue_append(q, t);
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_PROGRESS, _("%s: requeued (by %s:%u)"), desctask(t), file, line);
#endif
}

static void _task_1_queue_stats(QUEUE *q) {
#if DEBUG_ENABLED
  char		*msg = NULL;
  int		msgsize = 512;
  int		msglen = 0;
  TASK		*t = NULL;

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

  Debug(taskobj, DEBUGLEVEL_PROGRESS,
	_(
#if !DISABLE_DATE_LOGGING
	  "%s+%06lu "
#endif
	  "%s size=%u, max size=%u"),
#if !DISABLE_DATE_LOGGING
	datebuf, tv.tv_usec,
#endif
	q->queuename, (unsigned int)q->size, (unsigned int)q->max_size);
	  
  msg = ALLOCATE(msgsize, char*);

  msg[0] = '\0';
  for (t = (TASK*)q->head; t; t = task_next(t)) {
    int idsize;
    idsize = snprintf(&msg[msglen], msgsize - msglen, " %u", t->internal_id);
    msglen += idsize;
    if ((msglen + 2*idsize) >= msgsize) msg = REALLOCATE(msg, msgsize *= 2, char*);
  }
  if (msglen)
    Debug(taskobj, DEBUGLEVEL_PROGRESS, _("Queued tasks %s"), msg);

  RELEASE(msg);
#endif
}

void task_queue_stats() {
  int i = 0, j = 0;

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_queue_stats called"));
#endif
  for (i = NORMAL_TASK; i <= PERIODIC_TASK; i++) {
    for (j= HIGH_PRIORITY_TASK; j <= LOW_PRIORITY_TASK; j++) {
      _task_1_queue_stats(TaskArray[j][i]);
    }
  }
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_queue_stats returns"));
#endif
}

static void _task_close_queue(QUEUE *TaskP) {

  register TASK *t = NULL;

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("_task_close_queue called"));
#endif
  /* Close any TCP connections and any NOTIFY sockets */
  for (t = (TASK*)(TaskP->head); t; t = task_next(t)) {
    if (t->protocol == SOCK_STREAM && t->fd >= 0)
      sockclose(t->fd);
    else if (t->protocol == SOCK_DGRAM
	     && (t->status & (ReqTask|TickTask|RunTask))
	     && t->fd >= 0)
      sockclose(t->fd);
    dequeue(t);
  }
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("_task_close_queue returns"));
#endif
}

void task_free_all() {
  int i = 0, j = 0;

#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_free_all called"));
#endif
  for (i = NORMAL_TASK; i <= PERIODIC_TASK; i++) {
    for (j = HIGH_PRIORITY_TASK; j <= LOW_PRIORITY_TASK; j++) {
      QUEUE *TaskQ = TaskArray[i][j];
      _task_close_queue(TaskQ);
    }
  }
#if DEBUG_ENABLED
  Debug(taskobj, DEBUGLEVEL_FUNCS, _("task_free_all returns"));
#endif
}

/* vi:set ts=3: */
/* NEED_PO */
