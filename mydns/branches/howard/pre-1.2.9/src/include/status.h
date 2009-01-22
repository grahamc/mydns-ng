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

#ifndef _MYDNS_LIB_STATUS_H
#define _MYDNS_LIB_STATUS_H

#include "taskobj.h"

#define MAX_RESULTS	20
typedef struct _serverstatus					/* Server status information */
{
	time_t		start_time; 				/* Time server started */
	uint32_t	udp_requests, tcp_requests;		/* Total # of requests handled */
	uint32_t	timedout;	 			/* Number of requests that timed out */
	uint32_t	results[MAX_RESULTS];			/* Result codes */
} SERVERSTATUS;

extern SERVERSTATUS Status;

extern taskexec_t status_get_rr(TASK *t);
extern void status_task_timedout(TASK *t);
extern void status_start_server();
extern void status_tcp_request(TASK *t);
extern void status_udp_request(TASK *t);
extern void status_result(TASK *t, int rcode);

#define status_start_time()	(Status.start_time)
#define status_udp_requests()	(Status.udp_requests)
#define status_tcp_requests()	(Status.tcp_requests)
#define status_timeouts()	(Status.timedout)

#endif
