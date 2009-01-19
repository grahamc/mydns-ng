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


/* vi:set ts=3: */
/* NEED_PO */
