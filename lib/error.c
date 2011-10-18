/**************************************************************************************************
	$Id: error.c,v 1.42 2005/04/20 16:49:11 bboy Exp $

	error.c: Error reporting and message output functions.

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

#include "mydnsutil.h"

#define ERROR_MAXMSGLEN	1024

/* The name of the currently running program */
char	const *progname = (char *)PACKAGE;
int	mypid = -1;

/* Should ERR_VERBOSE cause output? */
int	err_verbose = 0;

#if DEBUG_ENABLED
/* Should ERR_DEBUG cause output? */
int	err_debug = 0;
#endif

/* Should error functions output to a file? */
FILE	*err_file = NULL;

/**************************************************************************************************
	ERROR_INIT
	Initialize error reporting and output functions.
**************************************************************************************************/
void
error_reinit() {
  mypid = getpid();
}

void
error_init(const char *argv0, int facility) {
  int option;

  /* Save program name */
  if (argv0) {
    char *c;
    if ((c = strrchr(argv0, '/')))
      progname = STRDUP(c+1);
    else
      progname = STRDUP((char *)argv0);
  }

  mypid = (int)getpid();

  /* Open syslog */
  if (!err_file) {
    option = LOG_CONS | LOG_NDELAY | LOG_PID;
#ifdef LOG_PERROR
    if (isatty(STDERR_FILENO))
      option |= LOG_PERROR;
#endif
    openlog(progname, option, facility);
  }
}
/*--- error_init() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	_ERROR_OUT
	This function handles all program output after command line handling.
	Always returns -1.
**************************************************************************************************/
static void
__error_out(int priority, const char *out, char **err_last) {
  static int  repeat = 0;

  if (err_last) {
    /* Don't output the same error message again and again */
    if (*err_last && !strcmp(out, *err_last)) {
      repeat++;
      return;
    }
    if (repeat) {
      if (err_file)
	fprintf(err_file, _("%s[%d]: last message repeated %d times\n"), progname, mypid, repeat + 1);
      else
	syslog(priority, _("last message repeated %d times"), repeat + 1);
    }
    repeat = 0;
    RELEASE(*err_last);
    *err_last = STRDUP(out);
  }
  if (err_file) {
    fprintf(err_file, _("%s[%d]: %s\n"), progname, mypid, out);
    fflush(err_file);
  } else {
    syslog(priority, "%s", out);
  }
}

static void
_error_out(
	int priority,		/* Priority (i.e. LOG_ERR, LOG_WARNING, LOG_NOTICE, LOG_INFO, LOG_DEBUG) */
	int errappend,		/* Append strerror(errno)? */
	int isfatal,		/* Is this error fatal? */
	const char *msg 
) {
  /* The last error message output, so we don't repeat ourselves */
  static char *err_last = NULL;
  char *out = NULL;

  /* Construct 'out' - the output */
#if SHOW_PID_IN_ERRORS
  if (err_file && errappend)
    ASPRINTF(&out, "%s [%d]: %s: %s", progname, getpid(), msg, strerror(errno));
  else if (err_file)
    ASPRINTF(&out, "%s [%d]: %s", progname, getpid(), msg);
#else
  if (err_file && errappend)
    ASPRINTF(&out, "%s: %s: %s", progname, msg, strerror(errno));
  else if (err_file)
    ASPRINTF(&out, "%s: %s", progname, msg);
#endif
  else if (errappend)
    ASPRINTF(&out, "%s: %s", msg, strerror(errno));
  else
    ASPRINTF(&out, "%s", msg);

  __error_out(priority, out, &err_last);

  RELEASE(out);

  /* Abort if the error should be fatal */
  if (isfatal)
    exit(EXIT_FAILURE);
}
/*--- _error_out() ------------------------------------------------------------------------------*/


#if DEBUG_ENABLED
/**************************************************************************************************
	DEBUG
**************************************************************************************************/
void
Debug(const char *fmt, ...) {
  char *msg = NULL;
  va_list ap;

  if (!err_debug) return;

  /* Construct output string */
  va_start(ap, fmt);
  VASPRINTF(&msg, fmt, ap);
  va_end(ap);

  _error_out(LOG_DEBUG, 0, 0, msg);

  RELEASE(msg);
}
/*--- Debug() -----------------------------------------------------------------------------------*/

/**************************************************************************************************
	DEBUGX
**************************************************************************************************/
void
DebugX(const char *debugId, int debugLvl, const char *fmt, ...) {
  char *msg = NULL;
  va_list ap;
  int debugEnabled = atou(conf_get(&Conf, "debug-enabled", NULL));
  int allEnabled = atou(conf_get(&Conf, "debug-all", NULL));
  int levelEnabled = 0;
  char *level = (char*)ALLOCATE(strlen(debugId) + sizeof("debug-") + 1, (char*));

  level[0] = '\0';
  strcpy(level, "debug-");
  strcat(level, debugId);

  levelEnabled = atou(conf_get(&Conf, level, NULL));

  RELEASE(level);

  if (debugEnabled <= 0) return;
  if (levelEnabled < debugLvl && allEnabled == 0) return;

  /* Construct output string */
  va_start(ap, fmt);
  VASPRINTF(&msg, fmt, ap);
  va_end(ap);

  _error_out(LOG_DEBUG, 0, 0, msg);

  RELEASE(msg);
}
/*--- DebugX() -----------------------------------------------------------------------------------*/
#endif


/**************************************************************************************************
	VERBOSE
**************************************************************************************************/
void
Verbose(const char *fmt, ...) {
  char *msg = NULL;
  va_list ap;

  if (!err_verbose) return;

  /* Construct output string */
  va_start(ap, fmt);
  VASPRINTF(&msg, fmt, ap);
  va_end(ap);

  _error_out(LOG_WARNING, 0, 0, msg);

  RELEASE(msg);
}
/*--- Verbose() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	NOTICE
**************************************************************************************************/
void
Notice(const char *fmt, ...) {
  char *msg = NULL;
  va_list ap;

  /* Construct output string */
  va_start(ap, fmt);
  VASPRINTF(&msg, fmt, ap);
  va_end(ap);

  _error_out(LOG_WARNING, 0, 0, msg);

  RELEASE(msg);
}
/*--- Notice() ----------------------------------------------------------------------------------*/


/**************************************************************************************************
	WARN
**************************************************************************************************/
int
Warn(const char *fmt, ...) {
  char *msg = NULL;
  va_list ap;

  /* Construct output string */
  va_start(ap, fmt);
  VASPRINTF(&msg, fmt, ap);
  va_end(ap);

  _error_out(LOG_WARNING, 1, 0, msg);

  RELEASE(msg);

  return (-1);
}
/*--- Warn() ------------------------------------------------------------------------------------*/


/**************************************************************************************************
	WARNX
**************************************************************************************************/
int
Warnx(const char *fmt, ...) {
  char *msg = NULL;
  va_list ap;

  /* Construct output string */
  va_start(ap, fmt);
  VASPRINTF(&msg, fmt, ap);
  va_end(ap);

  _error_out(LOG_WARNING, 0, 0, msg);

  RELEASE(msg);

  return (-1);
}
/*--- Warnx() -----------------------------------------------------------------------------------*/


/**************************************************************************************************
	ERR
**************************************************************************************************/
void
Err(const char *fmt, ...) {
  char *msg = NULL;
  va_list ap;

  /* Construct output string */
  va_start(ap, fmt);
  VASPRINTF(&msg, fmt, ap);
  va_end(ap);

  _error_out(LOG_ERR, 1, 1, msg);

  RELEASE(msg);
}
/*--- Err() -------------------------------------------------------------------------------------*/

void
Out_Of_Memory() {
  __error_out(LOG_ERR, _("out of memory"), NULL);
  abort();
}

/**************************************************************************************************
	ERRX
**************************************************************************************************/
void
Errx(const char *fmt, ...) {
  char *msg = NULL;
  va_list ap;

  /* Construct output string */
  va_start(ap, fmt);
  VASPRINTF(&msg, fmt, ap);
  va_end(ap);

  _error_out(LOG_ERR, 0, 1, msg);

  RELEASE(msg);
}
/*--- Errx() ------------------------------------------------------------------------------------*/


/**************************************************************************************************
	WARNSQL
	Outputs a warning caused by an SQL query failure.
**************************************************************************************************/
int
WarnSQL(
#if USE_PGSQL
	PGconn *pgsql,
#else
	MYSQL *mysql,
#endif
	const char *fmt, ...) {
  char *msg = NULL, *out = NULL;
  va_list ap;

  /* Construct output string */
  va_start(ap, fmt);
  VASPRINTF(&msg, fmt, ap);
  va_end(ap);

#if USE_PGSQL
  {
    char *errmsg = PQerrorMessage(pgsql), *c;

    for (c = errmsg; *c; c++)
      if (*c == '\r' || *c == '\n')
	*c = ' ';
    strtrim(errmsg);

    ASPRINTF(&out, "%s: %s (errno=%d)", msg, errmsg, errno);
  }
#else
  ASPRINTF(&out, "%s: %s (errno=%d)", msg, mysql_error(mysql), errno);
#endif
  RELEASE(msg);

  _error_out(LOG_WARNING, 0, 0, out);

  RELEASE(out);

  return (-1);
}
/*--- WarnSQL() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	ERRSQL
**************************************************************************************************/
void
ErrSQL(
#if USE_PGSQL
       PGconn *pgsql,
#else
       MYSQL *mysql,
#endif
       const char *fmt, ...) {
  char *msg = NULL, *out = NULL;
  va_list ap;

  /* Construct output string */
  va_start(ap, fmt);
  VASPRINTF(&msg, fmt, ap);
  va_end(ap);

#if USE_PGSQL
  {
    char *errmsg = PQerrorMessage(pgsql), *c;

    for (c = errmsg; *c; c++)
      if (*c == '\r' || *c == '\n')
	*c = ' ';
    strtrim(errmsg);

    ASPRINTF(&out, "%s: %s (errno=%d)", msg, errmsg, errno);
  }
#else
  ASPRINTF(&out, "%s: %s (errno=%d)", msg, mysql_error(mysql), errno);
#endif
  RELEASE(msg);

  _error_out(LOG_ERR, 0, 1, out);

  RELEASE(out);
}
/*--- ErrSQL() ----------------------------------------------------------------------------------*/

/* vi:set ts=3: */
