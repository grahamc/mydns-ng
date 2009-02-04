/**************************************************************************************************
	$Id: task.h,v 1.18 2005/04/20 16:49:12 bboy Exp $

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

#ifndef _MYDNS_TASK_H
#define _MYDNS_TASK_H

#include "taskobj.h"

extern int		task_timedout(TASK *);
extern taskexec_t	task_new(TASK *, unsigned char *, size_t);
extern void		task_init_header(TASK *);
extern int		task_process(TASK *, int, int, int);
extern void		task_start(void);
extern void		task_free_others(TASK *t, int closeallfds);
extern void		task_schedule_all(struct pollfd *items[], int *timeoutWanted, int *numfds, int *maxnumfds);
extern int		task_run_all(struct pollfd items[], int numfds);

#endif /* !_MYDNS_TASK_H */
/* vi:set ts=3: */
