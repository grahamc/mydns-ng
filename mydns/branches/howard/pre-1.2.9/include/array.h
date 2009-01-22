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

#ifndef _ARRAY_H
# define _ARRAY_H

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Structure for ARRAY objects */
typedef struct _named_array {
  size_t	size;
  int		maxidx;
  void		**objects;
} ARRAY;

extern ARRAY		*array_init(size_t);
extern void		array_free(ARRAY *, int);
extern void		array_append(ARRAY *, void *);
extern void		*array_remove(ARRAY *);
#define array_fetch(A,I)	(((A)->maxidx>=(I))?((A)->objects[(I)]):NULL)
#define array_store(A,I,O)	((A)->objects[(I)] = (O))
#define array_max(A)		((A)->maxidx)
#define array_numobjects(A)	(array_max((A))+1)

#ifdef __cplusplus
}
#endif

#endif
