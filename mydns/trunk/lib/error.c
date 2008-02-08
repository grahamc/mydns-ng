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
char	*progname = (char *)PACKAGE;

/* Should ERR_VERBOSE cause output? */
int	err_verbose = 0;

#if DEBUG_ENABLED
/* Should ERR_DEBUG cause output? */
int	err_debug = 0;
#endif

/* Should error functions output to a file? */
FILE	*err_file = NULL;

/* The last error message output, so we don't repeat ourselves */
static char err_last[ERROR_MAXMSGLEN + 1] = "";


/**************************************************************************************************
	ERROR_INIT
	Initialize error reporting and output functions.
**************************************************************************************************/
void
error_init(argv0, facility)
	const char *argv0;	/* argv[0] from calling program */
	int facility;			/* Logging facility (i.e. LOG_DAEMON, etc) */
{
	int option;

	/* Save program name */
	if (argv0)
	{
		char *c;
		if ((c = strrchr(argv0, '/')))
			progname = strdup(c+1);
		else
			progname = strdup((char *)argv0);
		if (!progname)
			Err("strdup");
	}

	/* Open syslog */
	if (!err_file)
	{
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
_error_out(
	int priority,				/* Priority (i.e. LOG_ERR, LOG_WARNING, LOG_NOTICE, LOG_INFO, LOG_DEBUG) */
	int errappend,				/* Append strerror(errno)? */
	int isfatal,				/* Is this error fatal? */
	const char *msg 
)
{
	static char out[ERROR_MAXMSGLEN + 2];
	static int  repeat = 0;
	int len;

	/* Construct 'out' - the output */
#if SHOW_PID_IN_ERRORS
	if (err_file && errappend)
		len = snprintf(out, sizeof(out) - 2, "%s [%d]: %s: %s", progname, getpid(), msg, strerror(errno));
	else if (err_file)
		len = snprintf(out, sizeof(out) - 2, "%s [%d]: %s", progname, getpid(), msg);
#else
	if (err_file && errappend)
		len = snprintf(out, sizeof(out) - 2, "%s: %s: %s", progname, msg, strerror(errno));
	else if (err_file)
		len = snprintf(out, sizeof(out) - 2, "%s: %s", progname, msg);
#endif
	else if (errappend)
		len = snprintf(out, sizeof(out) - 2, "%s: %s", msg, strerror(errno));
	else
		len = snprintf(out, sizeof(out) - 2, "%s", msg);

	/* Don't output the same error message again and again */
	if (strcmp(out, err_last))
	{
		if (repeat)
		{
			if (err_file)
				fprintf(err_file, _("%s: last message repeated %d times\n"), progname, repeat + 1);
			else
				syslog(priority, _("last message repeated %d times"), repeat + 1);
		}
		repeat = 0;
		strncpy(err_last, out, ERROR_MAXMSGLEN);
		if (err_file)
		{
			out[len++] = '\n';
			out[len] = '\0';
			fwrite(out, len, 1, err_file);
			fflush(err_file);
		}
		else
			syslog(priority, "%s", out);
	}
	else
		repeat++;

	/* Abort if the error should be fatal */
	if (isfatal)
		exit(EXIT_FAILURE);
}
/*--- _error_out() ------------------------------------------------------------------------------*/


/**************************************************************************************************
   _ERROR_ASSERT_FAIL
   This is called by the macro "Assert" if an assertion fails.
**************************************************************************************************/
void
_error_assert_fail(const char *assertion)
{
	char msg[BUFSIZ];

	snprintf(msg, sizeof(msg), "assertion failed: %s", assertion);

   _error_out(LOG_ERR, 0, 1, msg);
}
/*--- _error_assert_fail() ----------------------------------------------------------------------*/


#if DEBUG_ENABLED
/**************************************************************************************************
	DEBUG
**************************************************************************************************/
void
Debug(const char *fmt, ...)
{
	char msg[BUFSIZ];
	va_list ap;

	if (!err_debug)
		return;

	/* Construct output string */
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	_error_out(LOG_DEBUG, 0, 0, msg);
}
/*--- Debug() -----------------------------------------------------------------------------------*/
#endif


/**************************************************************************************************
	VERBOSE
**************************************************************************************************/
void
Verbose(const char *fmt, ...)
{
	char msg[BUFSIZ];
	va_list ap;

	if (!err_verbose)
		return;

	/* Construct output string */
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	_error_out(LOG_WARNING, 0, 0, msg);
}
/*--- Verbose() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	NOTICE
**************************************************************************************************/
void
Notice(const char *fmt, ...)
{
	char msg[BUFSIZ];
	va_list ap;

	/* Construct output string */
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	_error_out(LOG_WARNING, 0, 0, msg);
}
/*--- Notice() ----------------------------------------------------------------------------------*/


/**************************************************************************************************
	WARN
**************************************************************************************************/
int
Warn(const char *fmt, ...)
{
	char msg[BUFSIZ];
	va_list ap;

	/* Construct output string */
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	_error_out(LOG_WARNING, 1, 0, msg);
	return (-1);
}
/*--- Warn() ------------------------------------------------------------------------------------*/


/**************************************************************************************************
	WARNX
**************************************************************************************************/
int
Warnx(const char *fmt, ...)
{
	char msg[BUFSIZ];
	va_list ap;

	/* Construct output string */
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	_error_out(LOG_WARNING, 0, 0, msg);
	return (-1);
}
/*--- Warnx() -----------------------------------------------------------------------------------*/


/**************************************************************************************************
	ERR
**************************************************************************************************/
void
Err(const char *fmt, ...)
{
	char msg[BUFSIZ];
	va_list ap;

	/* Construct output string */
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	_error_out(LOG_ERR, 1, 1, msg);
}
/*--- Err() -------------------------------------------------------------------------------------*/


/**************************************************************************************************
	ERRX
**************************************************************************************************/
void
Errx(const char *fmt, ...)
{
	char msg[BUFSIZ];
	va_list ap;

	/* Construct output string */
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	_error_out(LOG_ERR, 0, 1, msg);
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
	const char *fmt, ...)
{
	char msg[BUFSIZ], out[BUFSIZ * 2];
	va_list ap;

	/* Construct output string */
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

#if USE_PGSQL
	{
		char *errmsg = PQerrorMessage(pgsql), *c;

		for (c = errmsg; *c; c++)
			if (*c == '\r' || *c == '\n')
				*c = ' ';
		strtrim(errmsg);

		snprintf(out, sizeof(out), "%s: %s (errno=%d)", msg, errmsg, errno);
	}
#else
	snprintf(out, sizeof(out), "%s: %s (errno=%d)", msg, mysql_error(mysql), errno);
#endif
	_error_out(LOG_WARNING, 0, 0, out);

	return (-1);
}
/*--- WarnSQL() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	SQL_ERRMSG
**************************************************************************************************/
char *
sql_errmsg(
#if USE_PGSQL
	PGconn *pgsql
#else
	MYSQL *mysql
#endif
)
{
	static char msg[BUFSIZ];

#if USE_PGSQL
	{
		char *errmsg = PQerrorMessage(pgsql), *c;

		for (c = errmsg; *c; c++)
			if (*c == '\r' || *c == '\n')
				*c = ' ';
		strtrim(errmsg);

		snprintf(msg, sizeof(msg), "%s (errno=%d)", errmsg, errno);
	}
#else
	snprintf(msg, sizeof(msg), "%s (errno=%d)", mysql_error(mysql), errno);
#endif
	return (msg);
}
/*--- sql_errmsg() ------------------------------------------------------------------------------*/


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
	const char *fmt, ...)
{
	char msg[BUFSIZ], out[BUFSIZ * 2];
	va_list ap;

	/* Construct output string */
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

#if USE_PGSQL
	{
		char *errmsg = PQerrorMessage(pgsql), *c;

		for (c = errmsg; *c; c++)
			if (*c == '\r' || *c == '\n')
				*c = ' ';
		strtrim(errmsg);

		snprintf(out, sizeof(out), "%s: %s (errno=%d)", msg, errmsg, errno);
	}
#else
	snprintf(out, sizeof(out), "%s: %s (errno=%d)", msg, mysql_error(mysql), errno);
#endif

	_error_out(LOG_ERR, 0, 1, out);
}
/*--- ErrSQL() ----------------------------------------------------------------------------------*/

/* vi:set ts=3: */
