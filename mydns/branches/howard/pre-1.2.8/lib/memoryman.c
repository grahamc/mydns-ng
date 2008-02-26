/*
 * memoryman.c -- Simple memory allocation abstraction
 *
 * This file provides an abstraction from the underlying
 * memory allocation facilities used by MyDNS
 *
 * It has been put here to provide hooks for changing memory
 * management facilities to experiement with different libraries;
 * to allow arena based storage allocation to use a common
 * abstraction; and to give a location for statistic gathering
 * and reporting
*/

/* To include macros and support definitions */
#include "mydnsutil.h"

#define DEBUG_MEMMAN 0

static char *
__mydns_arenaname(arena_t arena) {
  if (arena == ARENA_GLOBAL) { return "global"; }
  if (arena == ARENA_LOCAL) { return "local"; }
  return "SHARED";
}

int
_mydns_asprintf(char **strp, const char *fmt, ...) {
  int reslength;
  va_list ap;

#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("asprintf() called with fmt %s", fmt);
#endif

  va_start(ap, fmt);
  reslength = vasprintf(strp, fmt, ap);
  va_end(ap);

#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("asprintf() %s", (reslength>0)?_("succeeded"):_("failed"));
#endif

  if (reslength<0) Out_Of_Memory();

  return (reslength);
}

int
_mydns_vasprintf(char **strp, const char *fmt, va_list ap) {
  int reslength;

#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("asprintf() called with fmt %s", fmt);
#endif

  reslength = vasprintf(strp, fmt, ap);

#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("asprintf() %s", (reslength>0)?_("suceeded"):_("failed"));
#endif

  if (reslength<0) Out_Of_Memory();

  return (reslength);
}

char *
_mydns_strdup(const char *s, arena_t arena, char *file, int line) {
  char *news = NULL;

#if DEBUG_ENABLED && DEBUG_MEMMAN
  size_t size = strlen(s);
  Debug("Allocate string copy of size %d bytes, in arena %s from %s:%d",
	size, __mydns_arenaname(arena), file, line);
#endif

  news = strdup(s);

#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("Allocating string copy of size %d bytes in arena %s %s -> %p",
	size, __mydns_arenaname(arena), (news)?_("succeeded"):_("failed"), news);
#endif

  if (!news) Out_Of_Memory();

  return (news);
}

char *
_mydns_strndup(const char *s, size_t size, arena_t arena, char *file, int line) {
  char *news = NULL;

#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("Allocate string copy of size %d bytes, in arena %s from %s:%d",
	size, __mydns_arenaname(arena), file, line);
#endif

  news = strndup(s, size);

#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("Allocating string copy of size %d bytes in arena %s %s -> %p",
	size, __mydns_arenaname(arena), (news)?_("succeeded"):_("failed"), news);
#endif

  if (!news) Out_Of_Memory();

  return (news);
}

void *
_mydns_allocate(size_t size, size_t count, arena_t arena, char *type, char *file, int line) {

  void *newobject = NULL;
  /* We currently ignore arena which is there to allow shared memory or local allocation */
#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("Allocate %d units of memory of size %d, in arena %s, for %s objects from %s:%d",
	count, size, __mydns_arenaname(arena), type, file, line);
#endif

  newobject = calloc(count, size);

#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("Allocating %d unit of %d bytes in arena %s %s -> %p",
	count, size, __mydns_arenaname(arena), (newobject)?_("succeeded"):_("failed"), newobject);
#endif

  if (!newobject) Out_Of_Memory();

  return (newobject);
}

void *
_mydns_reallocate(void *oldobject, size_t size, size_t count, arena_t arena, char *type, char *file, int line) {
  void *newobject = NULL;

  /* We currently ignore arena which is there to allow shared memory or local allocation */
#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("Reallocate object %p as %d units of memory of size %d, in arena %s, for %s objects from %s:%d",
	oldobject, count, size, __mydns_arenaname(arena), type, file, line);
#endif

  newobject = realloc(oldobject, count * size);

#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("Reallocating %p as %d bytes in arena %s %s -> %p",
	oldobject, size, __mydns_arenaname(arena), (newobject)?_("succeeded"):_("failed"), newobject);
#endif

  if (!newobject) Out_Of_Memory();

  return (newobject);
}

void
_mydns_release(void *object, size_t count, arena_t arena, char *file, int line) {

  /* We currently ignore arena which is there to allow shared memory or local allocation */
#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("Release %d units of memory in arena %s, for object %p from %s:%d",
	count, __mydns_arenaname(arena), object, file, line);
#endif

  if (object) free(object); /* Use the system free directly - do not rely on the if NULL behaviour */

#if DEBUG_ENABLED && DEBUG_MEMMAN
  Debug("Released %d units of memory in arena %s, for object %p from %s:%d",
	count, __mydns_arenaname(arena), object, file, line);
#endif
}

/* vi:set ts=3: */
