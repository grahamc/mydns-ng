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

#ifndef _MYDNS_LIB_SUPPORT_H
#define _MYDNS_LIB_SUPPORT_H

#include <time.h>
#include <sys/time.h>

#include "array.h"
#include "taskobj.h"

#define DOMAINPORT 53   /* Port for notification servers */

extern time_t		current_time;			/* Current time */

extern struct timeval	*gettick(void);

extern int 		shutting_down;		/* Shutdown in progress? */

extern void		named_cleanup(int signo);
extern void		named_shutdown(int);
extern void		server_status(void);
extern char		*mydns_name_2_shortname(char *name, char *origin, int empty_name_is_ok, int notrim);
extern int		name_servers2ip(TASK *, ARRAY *, ARRAY *, ARRAY *);

#endif
