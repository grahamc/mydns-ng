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

#ifndef _MYDNS_MYDNS_NOTIFY_H
#define _MYDNS_MYDNS_NOTIFY_H

typedef struct _notify_slave {
  int			replied;	/* Have we had a reply from the slave */
  int			retries;        /* How many retries have we made */
  time_t		lastsent;	/* Last message was sent then */
  struct sockaddr	slaveaddr;
} NOTIFYSLAVE;

extern taskexec_t	notify_write(TASK *);
extern taskexec_t	notify_read(TASK*);
extern void		notify_slaves(TASK *, MYDNS_SOA *);
extern void		notify_start(void);

#endif
