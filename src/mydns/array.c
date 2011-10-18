/**************************************************************************************************
	$Id: array.c,v 1.0 2007/10/04 11:10:00 howard Exp $

	Copyright (C) 2007  Howard Wilkinson <howard@cohtech.com>

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
#define	DEBUG_ARRAY	1

/**************************************************************************************************
	ARRAY_INIT
	Creates a new array and returns a pointer to it.
**************************************************************************************************/
ARRAY *
array_init(size_t initial_size) {
  ARRAY *a = NULL;

  a = ALLOCATE(sizeof(ARRAY), ARRAY);

  if (initial_size <= 0) initial_size = 1;

  a->objects = ALLOCATE_N(initial_size, sizeof(void*), void*);

  a->size = initial_size;
  a->maxidx = -1;

  return (a);
}
/*--- array_init() ------------------------------------------------------------------------------*/

void
array_free(ARRAY *a, int release_contents) {

  if (!a) return;

  if (release_contents) {
    int i;
    for (i = 0; i <= array_max(a); i++) {
      void *o = array_fetch(a, i);
      RELEASE(o);
    }
  }

  RELEASE(a->objects);
  a->objects = NULL;
  a->size = 0;
  a->maxidx = -1;
  RELEASE(a);
}

/**************************************************************************************************
	ARRAY_APPEND
	Appends an object to the array extending the array if necessary
**************************************************************************************************/
void
array_append(ARRAY *a, void *o) {

  size_t idx = a->maxidx + 1;

  if (idx >= a->size) {
    a->objects = (void**)REALLOCATE(a->objects, (a->size*2)*sizeof(void*), void*);
    a->size *= 2;
  }

  a->objects[idx] = o;
  a->maxidx = idx;
}

void *
array_remove(ARRAY *a) {
  void *o = NULL;

  if (a->maxidx >= 0) o = a->objects[a->maxidx--];

  return o;
}
