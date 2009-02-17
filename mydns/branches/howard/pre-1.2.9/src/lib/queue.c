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

#include "memoryman.h"
#include "queue.h"

#if DEBUG_ENABLED
int		debug_queue = 0;
#endif

/**************************************************************************************************
	QUEUE_INIT
	Creates a new queue and returns a pointer to it.
**************************************************************************************************/
QUEUE *
queue_init(char *typename, char *priorityname) {
  QUEUE		*q = NULL;
  int		queuenamelen = strlen(typename) + strlen(priorityname) + 3;
  char		*queuename = NULL;

#if DEBUG_ENABLED
  Debug(queue, DEBUGLEVEL_FUNCS, _("queue_init: called for %s/%s"), typename, priorityname);
#endif
  queuenamelen = ASPRINTF(&queuename, "%s %ss", priorityname, typename);
  q = ALLOCATE(sizeof(QUEUE), QUEUE*);
  q->size = q->max_size = 0;
  q->queuename = queuename;
  q->head = q->tail = NULL;
#if DEBUG_ENABLED
  Debug(queue, DEBUGLEVEL_FUNCS, _("queue_init: returns"));
#endif
  return (q);
}
/*--- queue_init() ------------------------------------------------------------------------------*/

void queue_append(QUEUE **q, void *t) {
  QueueEntry *qe = (QueueEntry*)t;

#if DEBUG_ENABLED
  Debug(queue, DEBUGLEVEL_FUNCS, _("queue_append: called"));
#endif
  qe->next = qe->prev = NULL;

  if ((*q)->head) {
    (*q)->tail->next = qe;
    qe->prev = (*q)->tail;
  } else {
    (*q)->head = qe;
  }
  (*q)->tail = qe;

  (*q)->size++;

  if ((*q)->max_size < (*q)->size) (*q)->max_size = (*q)->size;

  qe->Q = q;
#if DEBUG_ENABLED
  Debug(queue, DEBUGLEVEL_FUNCS, _("queue_append: returns"));
#endif
}

void queue_remove(QUEUE **q, void *t) {
  QueueEntry *qe = (QueueEntry*)t;

#if DEBUG_ENABLED
  Debug(queue, DEBUGLEVEL_FUNCS, _("queue_remove: called"));
#endif
  if (qe == (*q)->head) {
    (*q)->head = qe->next;
    if ((*q)->head == NULL) {
      (*q)->tail = NULL;
    } else {
      if (qe->next) ((QueueEntry*)qe->next)->prev = NULL;
    }
  } else {
    if (qe->prev) ((QueueEntry*)qe->prev)->next = qe->next;
    if (qe->next == NULL) {
      (*q)->tail = qe->prev;
    } else {
      ((QueueEntry*)qe->next)->prev = qe->prev;
    }
  }
  (*q)->size--;

  qe->next = qe->prev = NULL;
  qe->Q = NULL;
#if DEBUG_ENABLED
  Debug(queue, DEBUGLEVEL_FUNCS, _("queue_remove: returns"));
#endif
}

/* vi:set ts=3: */
