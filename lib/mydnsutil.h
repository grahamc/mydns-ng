/**************************************************************************************************
	$Id: mydnsutil.h,v 1.68 2005/04/28 18:43:46 bboy Exp $

	mydnsutil.h: Support library for the MyDNS package.

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

#ifndef MYDNSUTIL_H
#define MYDNSUTIL_H

#define _GNU_SOURCE

#include <config.h>

/* <unistd.h> should be included before any preprocessor test
   of _POSIX_VERSION.  */
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_INTTYPES_H
#	include <inttypes.h>
#endif

#include <stdio.h>
#include <sys/types.h>

#ifdef HAVE_LIMITS_H
#	include <limits.h>
#endif

#ifdef HAVE_STDARG_H
#	include <stdarg.h>
#endif

#include <ctype.h>
#include <syslog.h>

#ifdef HAVE_LOCALE_H
#	include <locale.h>
#endif

#ifndef HAVE_SETLOCALE
#	define setlocale(category,locale) /* empty */
#endif

/* For gettext (NLS).  */
#include "gettext.h"
#define _(String) gettext(String)
#define N_(String) (String)

#ifdef STDC_HEADERS
#	define getopt system_getopt
#	include <stdlib.h>
#	undef getopt
#else
extern char *getenv ();
#endif

#include <getopt.h>

/* Don't use bcopy!  Use memmove if source and destination may overlap,
   memcpy otherwise.  */
#ifdef HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#else
# include <strings.h>
char *memchr ();
#endif

#if HAVE_SIGNAL_H
#	include <signal.h>
#endif

#if HAVE_TERMIOS_H
#	include <termios.h>
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif
#ifdef VMS
#include <perror.h>
#endif

#ifndef HAVE_DECL_STRERROR
extern char *strerror ();
#endif

#ifndef HAVE_DECL_STRCASECMP
extern int strcasecmp ();
#endif

#ifndef HAVE_DECL_STRNCASECMP
extern int strncasecmp ();
#endif

#ifndef HAVE_DECL_STRCOLL
extern int strcoll ();
#endif

#include <sys/stat.h>
#ifdef STAT_MACROS_BROKEN
# undef S_ISDIR
#endif
#if !defined(S_ISDIR) && defined(S_IFDIR)
# define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif /* HAVE_SYS_FILE_H */

#ifndef O_RDONLY
/* Since <fcntl.h> is POSIX, prefer that to <sys/fcntl.h>.
   This also avoids some useless warnings on (at least) Linux.  */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else /* not HAVE_FCNTL_H */
#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif /* not HAVE_SYS_FCNTL_H */
#endif /* not HAVE_FCNTL_H */
#endif /* not O_RDONLY */

#ifdef HAVE_SYS_SOCKET_H
#	include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#	include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
#	include <arpa/inet.h>
#endif

#if HAVE_SYS_SOCKIO_H
#  include <sys/sockio.h>
#endif

#if USE_PGSQL
#	include <libpq-fe.h>
#       include <pgtypes_timestamp.h>

#ifdef PGSQL_VERSION
#	define  SQL_VERSION_STR	"PostgreSQL " PGSQL_VERSION
#else		/* !PGSQL_VERSION */
#	if HAVE_PGCONFIG
#		include <pg_config.h>
#	endif	/* HAVE_PGCONFIG */
#		ifdef PG_VERSION
#			define  SQL_VERSION_STR	"PostgreSQL " PG_VERSION
#		else
#			define  SQL_VERSION_STR	"PostgreSQL"
#		endif
#endif	/* !PGSQL_VERSION */
#else		/* !USE_PGSQL (MySQL) */
#	include <mysql.h>
#	include <errmsg.h>
#	define  SQL_VERSION_STR	"MySQL " MYSQL_SERVER_VERSION
#endif	/* !USE_PGSQL */


/* MS-DOS and similar non-Posix systems have some peculiarities:
    - they distinguish between binary and text files;
    - they use both `/' and `\\' as directory separator in file names;
    - they can have a drive letter X: prepended to a file name;
    - they have a separate root directory on each drive;
    - their filesystems are case-insensitive;
    - directories in environment variables (like INFOPATH) are separated
        by `;' rather than `:';
    - text files can have their lines ended either with \n or with \r\n pairs;

   These are all parameterized here except the last, which is
   handled by the source code as appropriate (mostly, in info/).  */
#ifndef O_BINARY
# ifdef _O_BINARY
#  define O_BINARY _O_BINARY
# else
#  define O_BINARY 0
# endif
#endif /* O_BINARY */

#if O_BINARY
# include <io.h>
# ifdef __MSDOS__
#  include <limits.h>
#  ifdef __DJGPP__
#   define HAVE_LONG_FILENAMES(dir)  (pathconf (dir, _PC_NAME_MAX) > 12)
#   define NULL_DEVICE	"/dev/null"
#  else  /* !__DJGPP__ */
#   define HAVE_LONG_FILENAMES(dir)  (0)
#   define NULL_DEVICE	"NUL"
#  endif /* !__DJGPP__ */
#  define SET_SCREEN_SIZE_HELPER terminal_prep_terminal()
#  define DEFAULT_INFO_PRINT_COMMAND ">PRN"
# else   /* !__MSDOS__ */
#  define setmode(f,m)  _setmode(f,m)
#  define HAVE_LONG_FILENAMES(dir)   (1)
#  define NULL_DEVICE	"NUL"
# endif  /* !__MSDOS__ */
# define SET_BINARY(f)  do {if (!isatty(f)) setmode(f,O_BINARY);} while(0)
# define FOPEN_RBIN	"rb"
# define FOPEN_WBIN	"wb"
# define IS_SLASH(c)	((c) == '/' || (c) == '\\')
# define HAVE_DRIVE(n)	((n)[0] && (n)[1] == ':')
# define IS_ABSOLUTE(n)	(IS_SLASH((n)[0]) || ((n)[0] && (n)[1] == ':'))
# define FILENAME_CMP	strcasecmp
# define FILENAME_CMPN	strncasecmp
# define PATH_SEP	";"
# define STRIP_DOT_EXE	1
# define DEFAULT_TMPDIR	"c:/"
# define PIPE_USE_FORK	0
#else  /* not O_BINARY */
# define SET_BINARY(f)	(void)0
# define FOPEN_RBIN	"r"
# define FOPEN_WBIN	"w"
# define IS_SLASH(c)	((c) == '/')
# define HAVE_DRIVE(n)	(0)
# define IS_ABSOLUTE(n)	((n)[0] == '/')
# define FILENAME_CMP	strcmp
# define FILENAME_CMPN	strncmp
# define HAVE_LONG_FILENAMES(dir)   (1)
# define PATH_SEP	":"
# define STRIP_DOT_EXE	0
# ifdef VMS
#  define DEFAULT_TMPDIR "sys$scratch:"
# else
#  define DEFAULT_TMPDIR "/tmp/"
# endif
# define NULL_DEVICE	"/dev/null"
# define PIPE_USE_FORK	1
#endif /* not O_BINARY */

#if HAVE_TIME_H
#	include <time.h>
#endif
#if HAVE_SYS_TIME_H
#	include <sys/time.h>
#endif

#if HAVE_PWD_H
#	include <pwd.h>
#endif

#if HAVE_POLL_H
#	include <poll.h>
#else
# if HAVE_SYS_SELECT_H
#	include <sys/select.h>
# endif

struct pollfd {
  int		fd;		/* file descriptor */
  short		events;		/* requested events */
  short		revents;	/* returned events */
};

#endif
/* Cope with OS's that do not declare some of these */
/* Input/Output values */
#ifndef POLLIN
#define POLLIN		0x0001
#endif
#ifndef POLLPRI
#define POLLPRI		0x0002
#endif
#ifndef POLLOUT
#define POLLOUT		0x0004
#endif
#ifndef POLLMSG
#define POLLMSG		0x0400
#endif
#ifndef POLLREMOVE
#define POLLREMOVE	0x1000
#endif
#ifndef POLLRDHUP
#define POLLRDHUP	0x2000
#endif
/* Output values */
#ifndef POLLERR
#define POLLERR		0x0008
#endif
#ifndef POLLHUP
#define POLLHUP		0x0010
#endif
#ifndef POLLNVAL
#define POLLNVAL	0x0020
#endif

#ifndef HAVE_UCHAR
typedef unsigned char uchar;
#endif

#ifndef HAVE_UINT
typedef unsigned int uint;
#endif

#ifndef HAVE_UINT8_T
typedef unsigned char uint8_t;
#endif

#ifndef HAVE_UINT16_T
typedef unsigned short uint16_t;
#endif

#ifndef HAVE_UINT32_T
typedef unsigned int uint32_t;
#endif


/* Placing this after a function prototype indicates to the compiler that the function takes a
   variable number of arguments in printf(3) format.  If this is used, the compiler will check
   to make sure your variables match the format string, just like it would with printf(3) and
   friends.  You shouldn't use this if it's possible to pass NULL as the fmtarg, since the
   the compiler will issue a warning about a NULL format string. */
/* GNU only... */
#define  __printflike(fmtarg, firstvararg) \
            __attribute__((__format__ (__printf__, fmtarg, firstvararg)))

/* Macro for pluralizing words in printf() statements.  If 'n' is 1, evaluates to "", otherwise
   evaluates to "s".  i.e. printf("%d number%s found.", num, S(num)); */
#define  S(n)  ((n == 1) ? "" : "s")

/* Evaluates to the number of items in the specified array. */
#define  NUM_ENTRIES(Array) (sizeof Array / sizeof *(Array))

/* Macro to return the last char in a string */
#define	LASTCHAR(s)	((s)[strlen((s))-1])


/* Macro to determine an average.  "t" is the total, and "c" is the number of elements. */
#define  AVG(t,c)    (double)(((double)c > 0.0) ? (double)((double)t / (double)c) : 0.0)

/* Macro to determine a percentage.  "t" is the total, "c" is the count. i.e. PCT(100,65) 
	evaluates as (double)65.0. */
#define  PCT(t,c)    (double)(((double)t > 0.0) ? (double)((double)c / (double)t) * 100.0 : 0.0)

/* Returns 1 for "[Yy](es)", "[Tt](rue)", "1", "[Aa]ctive", or "(o)[Nn]" */
#define  GETBOOL(str)   \
	((str) && ((str)[0] == 'Y' || (str)[0] == 'y' || (str)[0] == 'T' || (str)[0] == 't' || \
	 (str)[0] == '1' || (str)[1] == 'n' || (str)[1] == 'N' || (str)[0] == 'A'))

/* These characters might cause problems in SQL queries; they should be escaped (or the data
	rejected, which is probably always appropriate for DNS data */
#define	SQL_BADCHAR(c)		(((c) == '\n' || (c) == '\r' || (c) == '\\' || (c) == '\'' || (c) == '"'))


#ifndef  __STRING
#  define   __STRING(x) "x"
#endif
#if (!defined (__GNUC__) || __GNUC__ < 2 || __GNUC_MINOR__ < (defined (__cplusplus) ? 6 : 4))
#  define   __FUNCTION_NAME   ((const char *)0)
#else
#  define   __FUNCTION_NAME   __PRETTY_FUNCTION__
#endif

#ifndef MAX
#define MAX(__N__, __M__) (((__N__) > (__M__))?(__N__):(__M__))
#endif
#ifndef MIN
#define MIN(__N__, __M__) (((__N__) < (__M__))?(__N__):(__M__))
#endif

/* cidr.c */
extern int in_cidr(char *cidr, struct in_addr ip);


/*
**  conf.c
**  Routines to load the configuration file
*/
/* Generic structure for holding name/value pairs */
typedef struct _conflist {
  const char	*name;		/* Name of option */
  char		*value;		/* Value for this option */
  const char	*desc;		/* Description of this option */
  const char	*altname;	/* Alternate name for this option */
  int		defaulted;	/* This variable was defaulted; not actually in config file */
  struct _conflist *next;
} CONF;

#define	CONF_FS_CHAR	'\034'
#define	CONF_FS_STR		"\034"

extern void		conf_clobber(CONF **, const char *, const char *);
extern void		conf_set(CONF **, const char *, const char *, int);
extern const char	*conf_get(CONF **, const char *, int *);
extern void		conf_load(CONF **, const char *);

#define MEMMAN 1

#if MEMMAN == 0
#define __ALLOCATE__(SIZE, THING, COUNT, ARENA) calloc(COUNT, SIZE)
#define __REALLOCATE__(OBJECT, SIZE, THING, COUNT, ARENA) realloc(OBJECT, (COUNT)*(SIZE))
#define __RELEASE__(OBJECT, COUNT, ARENA)  if ((OBJECT)) free((OBJECT)), (OBJECT) = NULL
#define STRDUP(__STRING__) strdup(__STRING__)
#define STRNDUP(__STRING__, __LENGTH__) strndup(__STRING__, __LENGTH__)
#define ASPRINTF asprintf
#define VASPRINTF vasprintf
#else
#define __ALLOCATE__(SIZE, THING, COUNT, ARENA)	\
  _mydns_allocate(SIZE, COUNT, ARENA, "##THING##", __FILE__, __LINE__)
#define __REALLOCATE__(OBJECT, SIZE, THING, COUNT, ARENA) \
  _mydns_reallocate((void*)(OBJECT), SIZE, COUNT, ARENA, "##THING##", __FILE__, __LINE__)
#define __RELEASE__(OBJECT, COUNT, ARENA) \
  _mydns_release((void*)(OBJECT), COUNT, ARENA, __FILE__, __LINE__), (OBJECT) = NULL

#define STRDUP(__STRING__)		_mydns_strdup(__STRING__, ARENA_GLOBAL, __FILE__, __LINE__)
#define STRNDUP(__STRING__, __LENGTH__) _mydns_strndup(__STRING__, __LENGTH__, ARENA_GLOBAL, __FILE__, __LINE__)
#define ASPRINTF			_mydns_asprintf
#define VASPRINTF			_mydns_vasprintf

#endif

/*
**  memoryman.c
**  Memory management functions.
*/
typedef enum _arena_t {
  ARENA_GLOBAL		= 0,
  ARENA_LOCAL		= 1,
  ARENA_SHARED0		= 2,
} arena_t;

extern int	_mydns_asprintf(char **strp, const char *fmt, ...);
extern int	_mydns_vasprintf(char **strp, const char *fmt, va_list ap);
extern char *	_mydns_strdup(const char *, arena_t, const char *, int);
extern char *	_mydns_strndup(const char *, size_t, arena_t, const char *, int);
extern void *	_mydns_allocate(size_t, size_t, arena_t, const char *, const char *, int);
extern void *	_mydns_reallocate(void *, size_t, size_t, arena_t, const char *, const char *, int);
extern void	_mydns_release(void *, size_t, arena_t, const char *, int);

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

/* Convert str to unsigned int */
#define atou(s) (uint32_t)strtoul(s, (char **)NULL, 10)

extern CONF	*Conf;				/* Config file data */

/*
**  error.c
**  Error reporting functions.
*/
extern const char	*progname;			/* The name of this program */
extern int		err_verbose;			/* Should ERR_VERBOSE output anything? */
#if DEBUG_ENABLED
extern int		err_debug;			/* Should ERR_DEBUG output anything? */
#endif
extern FILE		*err_file;			/* Output to this file */

extern void		error_reinit(void);
extern void		error_init(const char *argv0, int facility);
#if DEBUG_ENABLED
extern void		Debug(const char *, ...) __printflike(1,2);
extern void		DebugX(const char *, int, const char *, ...) __printflike(3,4);
#endif
extern void		Verbose(const char *, ...) __printflike(1,2);
extern void		Notice(const char *, ...) __printflike(1,2);
extern int		Warn(const char *, ...) __printflike(1,2);
extern int		Warnx(const char *, ...) __printflike(1,2);
extern void		Err(const char *, ...) __printflike(1,2);
extern void		Errx(const char *, ...) __printflike(1,2);
extern void		Out_Of_Memory(void);

#if USE_PGSQL
extern int		WarnSQL(PGconn *, const char *, ...) __printflike(2,3);
extern void		ErrSQL(PGconn *, const char *, ...) __printflike(2,3);
#else
extern int		WarnSQL(MYSQL *, const char *, ...) __printflike(2,3);
extern void		ErrSQL(MYSQL *, const char *, ...) __printflike(2,3);
#endif

#if !HAVE_DECL_STRSEP
extern char		*strsep(char **stringp, const char *delim);
#endif

#if !HAVE_INET_PTON
extern int		inet_pton(int, const char *, void *);
#endif

#if !HAVE_INET_NTOP
extern const char *sql_errmsg(MYSQL *);
*inet_ntop(int, const void *, char *, unsigned int);
#endif


/* getoptstr.c */
extern char		*getoptstr(struct option const longopts[]);


/* ip.c */
extern void		_sockclose(int, const char*, int);
#define			sockclose(fd)	_sockclose((fd), __FILE__, __LINE__), (fd) = -1
extern const char	*ipaddr(int, void *);
#if HAVE_IPV6
extern int		is_ipv6(char *);
#endif


/* passinput.c */
extern char		*passinput(const char *prompt);

/* string.c */
extern char		*strtrimlead(char *), *strtrimtrail(char *), *strtrim(char *);
extern char		*strtoupper(char *), *strtolower(char *);
extern char		*strsecs(time_t);
extern char		*strdcat(char **, const char *);
extern int		sdprintf(char **, const char *, ...) __printflike(2,3);
extern size_t		human_file_size(const char *);

/* strsep_quotes.c */
extern char		*strsep_quotes(char **, char *, size_t);
extern int		strsep_quotes2(char **, char **);

/* wildcard.c */
extern int		wildcard_valid(char *p);
extern int		wildcard_match(register char *, register char *);

#endif /* MYDNSUTIL_H */

/* vi:set ts=3: */
