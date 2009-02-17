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

#ifndef _MYDNS_LIB_ERROR_H
#define _MYDNS_LIB_ERROR_H

extern taskexec_t	_formerr_internal(TASK *, dns_rcode_t, task_error_t, char *, const char *, unsigned int);
extern taskexec_t	_dnserror_internal(TASK *, dns_rcode_t, task_error_t, const char *, unsigned int);
extern char		*err_reason_str(TASK *, task_error_t);
extern int		rr_error(uint32_t, const char *, ...) __printflike(2,3);

#define formerr(task,rcode,reason,xtra)	_formerr_internal((task),(rcode),(reason),(xtra),__FILE__,__LINE__)
#define dnserror(task,rcode,reason)	_dnserror_internal((task),(rcode),(reason),__FILE__,__LINE__)

#endif
