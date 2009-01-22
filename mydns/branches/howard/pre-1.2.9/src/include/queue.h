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

#ifndef _MYDNS_LIB_QUEUE_H
#define _MYDNS_LIB_QUEUE_H

typedef struct __queue *QUEUEP;

typedef struct __queue_entry {
  QUEUEP		*Q;
  struct __queue_entry	*next;
  struct __queue_entry	*prev;
} QueueEntry;

typedef struct __queue
{
  char		*queuename;
  size_t	size;					/* Number of elements in queue */
  size_t	max_size;
  QueueEntry	*head;					/* Pointer to first element in list */
  QueueEntry	*tail;					/* Pointer to last element in list */
} QUEUE;

extern QUEUE		*queue_init(char *, char *);
extern void		queue_append(QUEUE **q, void *t);
extern void		queue_remove(QUEUE **q, void *t);

#endif

