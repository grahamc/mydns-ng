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
#include "memoryman.h"

#if DEBUG_ENABLED

int		debug_memoryman = 0;

static const char *
__mydns_arenaname(arena_t arena) {
  if (arena == ARENA_GLOBAL) { return "global"; }
  if (arena == ARENA_LOCAL) { return "local"; }
  return "SHARED";
}
#endif

int
_mydns_asprintf(char **strp, const char *fmt, ...) {
  int reslength;
  va_list ap;

#if DEBUG_ENABLED
  Debug(memoryman, DEBUGLEVEL_FULLTRACE, _("_mydns_asprintf called with format string %s"), fmt);
#endif

  va_start(ap, fmt);
  reslength = vasprintf(strp, fmt, ap);
  va_end(ap);

  if (reslength < 0) Out_Of_Memory();

  return (reslength);
}

int
_mydns_vasprintf(char **strp, const char *fmt, va_list ap) {
  int reslength;

#if DEBUG_ENABLED
  Debug(memoryman, DEBUGLEVEL_FULLTRACE, _("_mydns_vasprintf called with format string %s"), fmt);
#endif

  reslength = vasprintf(strp, fmt, ap);

  if (reslength < 0) Out_Of_Memory();

  return (reslength);
}

char *
_mydns_strdup(const char *s, arena_t arena, const char *file, int line) {
  char *news = NULL;

#if DEBUG_ENABLED
  Debug(memoryman, DEBUGLEVEL_FULLTRACE, _("_mydns_strdup called to duplicate %s for arena %s from %s(%d)"),
	s, __mydns_arenaname(arena), file, line);
#endif

#if HAVE_STRDUP
  news = strdup(s);
  if (!news) Out_Of_Memory();
#else
  int slen = strlen(s);
  news = (char*)_mydns_allocate(slen+1, 1, arena, "char *", file, line);
  if (!news) Out_Of_Memory();
  strncpy(news, s, slen);
  news[slen] = '\0';
#endif

  return (news);
}

char *
_mydns_strndup(const char *s, size_t size, arena_t arena, const char *file, int line) {
  char *news = NULL;

#if DEBUG_ENABLED
  Debug(memoryman, DEBUGLEVEL_FULLTRACE, _("_mydns_strndup called to duplicate %s(%d) for arena %s from %s(%d)"),
	s, size, __mydns_arenaname(arena), file, line);
#endif

#if HAVE_STRNDUP
  news = strndup(s, size);
  if (!news) Out_Of_Memory();
#else
  news = (char*)_mydns_allocate(size+1, 1, arena, "char *", file, line);
  if (!news) Out_Of_Memory();
  strncpy(news, s, size);
  news[size] = '\0';
#endif

  return (news);
}

void *
_mydns_allocate(size_t size, size_t count, arena_t arena, const char *type, const char *file, int line) {

  void *newobject = NULL;
  /* We currently ignore arena which is there to allow shared memory or local allocation */

#if DEBUG_ENABLED
  Debug(memoryman, DEBUGLEVEL_FULLTRACE, _("_mydns_allocate called to allocate %s %d * %d for arena %s from %s(%d)"),
	type, size, count, __mydns_arenaname(arena), file, line);
#endif

  newobject = calloc(count, size);

  if (!newobject) Out_Of_Memory();

  return (newobject);
}

void *
_mydns_reallocate(void *oldobject, size_t size, size_t count, arena_t arena, const char *type, const char *file, int line) {
  void *newobject = NULL;

  /* We currently ignore arena which is there to allow shared memory or local allocation */

#if DEBUG_ENABLED
  Debug(memoryman, DEBUGLEVEL_FULLTRACE, _("_mydns_reallocate called to reallocate %p as %s %d * %d for arena %s from %s(%d)"),
	oldobject, type, size, count, __mydns_arenaname(arena), file, line);
#endif

  newobject = realloc(oldobject, count * size);

  if (!newobject) Out_Of_Memory();

#if DEBUG_ENABLED
  Debug(memoryman, DEBUGLEVEL_FULLTRACE, _("_mydns_reallocate returns %p for arena %s from %s(%d)"),
	newobject, __mydns_arenaname(arena), file, line);
#endif

  return (newobject);
}

void
_mydns_release(void *object, arena_t arena, const char *file, int line) {

  /* We currently ignore arena which is there to allow shared memory or local allocation */

#if DEBUG_ENABLED
  Debug(memoryman, DEBUGLEVEL_FULLTRACE, _("_mydns_release called to release %p for arena %s from %s(%d)"),
	object, __mydns_arenaname(arena), file, line);
#endif

  if (object) free(object); /* Use the system free directly - do not rely on the if NULL behaviour */

}

/* vi:set ts=3: */
