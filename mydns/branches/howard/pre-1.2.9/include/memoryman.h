/**************************************************************************************************
	memoryman.h: Memory management support library for the MyDNS package.

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

#ifndef _MEMORYMAN_H
#define _MEMORYMAN_H 1

#include <unistd.h>
#include <stdarg.h>

#define __ALLOCATE__(SIZE, THING, COUNT, ARENA)	\
  _mydns_allocate(SIZE, COUNT, ARENA, "##THING##", __FILE__, __LINE__)
#define __REALLOCATE__(OBJECT, SIZE, THING, COUNT, ARENA) \
  _mydns_reallocate(OBJECT, SIZE, COUNT, ARENA, "##THING##", __FILE__, __LINE__)
#define __RELEASE__(OBJECT, COUNT, ARENA) \
  _mydns_release(OBJECT, COUNT, ARENA, __FILE__, __LINE__), (OBJECT) = NULL

#define STRDUP(__STRING__)		_mydns_strdup(__STRING__, ARENA_GLOBAL, __FILE__, __LINE__)
#define STRNDUP(__STRING__, __LENGTH__) _mydns_strndup(__STRING__, __LENGTH__, ARENA_GLOBAL, __FILE__, __LINE__)
#define ASPRINTF			_mydns_asprintf
#define VASPRINTF			_mydns_vasprintf

typedef enum _arena_t {
  ARENA_GLOBAL		= 0,
  ARENA_LOCAL		= 1,
  ARENA_SHARED0		= 2,
} arena_t;

extern int	_mydns_asprintf(char **strp, const char *fmt, ...);
extern int	_mydns_vasprintf(char **strp, const char *fmt, va_list ap);
extern char *	_mydns_strdup(const char *, arena_t, char *, int);
extern char *	_mydns_strndup(const char *, size_t, arena_t, char *, int);
extern void *	_mydns_allocate(size_t, size_t, arena_t, char *, char *, int);
extern void *	_mydns_reallocate(void *, size_t, size_t, arena_t, char *, char *, int);
extern void	_mydns_release(void *, size_t, arena_t, char *, int);

#define ALLOCATE_GLOBAL(SIZE, THING) \
  __ALLOCATE__(SIZE, THING, 1, ARENA_GLOBAL)
#define ALLOCATE_LOCAL(SIZE, THING) \
  __ALLOCATE__(SIZE, THING, 1, ARENA_LOCAL)
#define ALLOCATE_SHARED(SIZE, THING, POOL) \
  __ALLOCATE__(SIZE, THING, 1, ARENA_SHARED##POOL)

#define ALLOCATE_N_GLOBAL(COUNT, SIZE, THING) \
  __ALLOCATE__(SIZE, THING, COUNT, ARENA_GLOBAL)
#define ALLOCATE_N_LOCAL(COUNT, SIZE, THING) \
  __ALLOCATE__(SIZE, THING, COUNT, ARENA_LOCAL)
#define ALLOCATE_N_SHARED(COUNT, SIZE, THING, POOL) \
  __ALLOCATE__(SIZE, THING, COUNT, ARENA_SHARED##POOL)

#define ALLOCATE(SIZE, THING) \
  ALLOCATE_GLOBAL(SIZE, THING)
#define ALLOCATE_N(COUNT, SIZE, THING)	\
  ALLOCATE_N_GLOBAL(COUNT, SIZE, THING)

#define REALLOCATE_GLOBAL(OBJECT, SIZE, THING) \
  __REALLOCATE__(OBJECT, SIZE, THING, 1, ARENA_GLOBAL)
#define REALLOCATE_LOCAL(OBJECT, SIZE, THING) \
  __REALLOCATE__(OBJECT, SIZE, THING, 1, ARENA_LOCAL)
#define REALLOCATE_SHARED(OBJECT, SIZE, THING, POOL) \
  __REALLOCATE__(OBJECT, SIZE, THING, 1, ARENA_SHARED##POOL)

#define REALLOCATE(OBJECT, SIZE, THING)	\
  REALLOCATE_GLOBAL(OBJECT, SIZE, THING)

#define RELEASE_GLOBAL(OBJECT) \
  __RELEASE__(OBJECT, 1, ARENA_GLOBAL)
#define RELEASE_LOCAL(OBJECT) \
  __RELEASE__(OBJECT, 1, ARENA_LOCAL)
#define RELEASE_SHARED(OBJECT, POOL) \
  __RELEASE__(OBJECT, 1, ARENA_SHARED##POOL)

#define RELEASE(OBJECT)	\
  RELEASE_GLOBAL(OBJECT)

#endif

/* vi:set ts=3: */
