/**************************************************************************************************
	$Id: queue.c,v 1.33 2005/04/20 16:49:12 bboy Exp $

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
#define	DEBUG_QUEUE	1

static void
_queue_stats(QUEUE *q) {
#if DEBUG_ENABLED && DEBUG_QUEUE
  char msg[512];
  TASK *t;

#if !DISABLE_DATE_LOGGING
  struct timeval tv;
  time_t tt;
  struct tm *tm;
  char datebuf[80];

  gettimeofday(&tv, NULL);
  tt = tv.tv_sec;
  tm = localtime(&tt);

  strftime(datebuf, sizeof(datebuf)-1, "%d-%b-%Y %H:%M:%S", tm);
#endif

  Debug(
#if !DISABLE_DATE_LOGGING
	  "%s+%06lu "
#endif
	  "%s size=%d, max size=%d",
#if !DISABLE_DATE_LOGGING
	  datebuf, tv.tv_usec,
#endif
	  q->queuename, q->size, q->max_size);
	  
  msg[0] = '\0';
  for (t = q->head; t; t = t->next) {
    int msglen = strlen(msg);
    snprintf(&msg[msglen], sizeof(msg) - msglen, " %u", t->internal_id);
  }
  if (strlen(msg))
    Debug("Queued tasks %s", msg);
#endif
}

void
queue_stats() {
  int i,j;

  for (i = NORMAL_TASK; i <= PERIODIC_TASK; i++) {
    for (j= HIGH_PRIORITY_TASK; j <= LOW_PRIORITY_TASK; j++) {
      _queue_stats(TaskArray[j][i]);
    }
  }
}
/**************************************************************************************************
	QUEUE_INIT
	Creates a new queue and returns a pointer to it.
**************************************************************************************************/
QUEUE *
queue_init(char *typename, char *priorityname) {
  QUEUE *q;
  int queuenamelen = strlen(typename) + strlen(priorityname) + 3;
  char * queuename;

  queuenamelen = ASPRINTF(&queuename, "%s %ss", priorityname, typename);
  q = ALLOCATE(sizeof(QUEUE), QUEUE);
  q->size = q->max_size = 0;
  q->queuename = queuename;
  q->head = q->tail = (TASK *)NULL;
  return (q);
}
/*--- queue_init() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	_ENQUEUE
	Enqueues a TASK item, appending it to the end of the list.
**************************************************************************************************/
static void
__queue_append(QUEUE **q, TASK *t) {

  t->next = t->prev = NULL;

  if ((*q)->head) {
    (*q)->tail->next = t;
    t->prev = (*q)->tail;
  } else {
    (*q)->head = t;
  }
  (*q)->tail = t;

  (*q)->size++;

  if ((*q)->max_size < (*q)->size) (*q)->max_size = (*q)->size;

  t->TaskQ = q;
}

int
_enqueue(QUEUE **q, TASK *t, const char *file, unsigned int line) {

  __queue_append(q, t);

  t->len = 0;							/* Reset TCP packet len */

  if (t->protocol == SOCK_STREAM)
    Status.tcp_requests++;
  else if (t->protocol == SOCK_DGRAM)
    Status.udp_requests++;

#if DEBUG_ENABLED && DEBUG_QUEUE
  Debug("%s: enqueued (by %s:%u)", desctask(t), file, line);
#endif

  return (0);
}
/*--- _enqueue() --------------------------------------------------------------------------------*/


/**************************************************************************************************
	_DEQUEUE
	Removes the item specified from the queue.  Pass this a pointer to the actual element in the
	queue.
	For `error' pass 0 if the task was dequeued due to sucess, 1 if dequeued due to error.
**************************************************************************************************/
static void
__queue_remove(QUEUE **q, TASK *t) {

  if (t == (*q)->head) {
    (*q)->head = t->next;
    if ((*q)->head == NULL) {
      (*q)->tail = NULL;
    } else {
      if (t->next) t->next->prev = NULL;
    }
  } else {
    if (t->prev) t->prev->next = t->next;
    if (t->next == NULL) {
      (*q)->tail = t->prev;
    } else {
      t->next->prev = t->prev;
    }
  }
  (*q)->size--;

  t->next = t->prev = NULL;
  t->TaskQ = NULL;
}

void
_dequeue(QUEUE **q, TASK *t, const char *file, unsigned int line) {
#if DEBUG_ENABLED && DEBUG_QUEUE
  char *taskdesc = desctask(t);
#endif
  if (err_verbose)				/* Output task info if being verbose */
    task_output_info(t, NULL);

  if (t->hdr.rcode >= 0 && t->hdr.rcode < MAX_RESULTS)		/* Store results in stats */
    Status.results[t->hdr.rcode]++;

  __queue_remove(q, t);

  task_free(t);
#if DEBUG_ENABLED && DEBUG_QUEUE
  Debug("%s: dequeued (by %s:%u)", taskdesc, file, line);
#endif
}
/*--- _dequeue() --------------------------------------------------------------------------------*/

void
_requeue(QUEUE **q, TASK *t, const char *file, unsigned int line) {
#if DEBUG_ENABLED && DEBUG_QUEUE
  char *taskdesc = desctask(t);
  Debug("%s: requeuing (by %s:%u) called", taskdesc, file, line);
#endif

  __queue_remove(t->TaskQ, t);
  __queue_append(q, t);

}
/* vi:set ts=3: */
