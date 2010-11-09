/**************************************************************************************************
	$Id: conf.c,v 1.59 2006/01/18 20:46:46 bboy Exp $

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

#include "mydns.h"

/* Make this nonzero to enable debugging for this source file */
#define	DEBUG_CONF	1


#include <pwd.h>
#include <grp.h>

int		opt_daemon = 0;				/* Run in background? (-d, --daemon) */
const char	*opt_conf = MYDNS_CONF;			/* Location of config file (-c, --conf) */
uid_t		perms_uid = 0;				/* User permissions */
gid_t		perms_gid = 0;				/* Group permissions */
time_t		task_timeout;				/* Task timeout */
int		axfr_enabled = 0;			/* Enable AXFR? */
int		tcp_enabled = 0;			/* Enable TCP? */
int		dns_update_enabled = 0;			/* Enable DNS UPDATE? */
int		dns_notify_enabled = 0;			/* Enable notify */
int		notify_timeout = 60;
int		notify_retries = 5;
const char	*notify_algorithm = "linear";

int		dns_ixfr_enabled = 0;			/* Enable IXFR functionality */
int		ixfr_gc_enabled = 0;			/* Enable IXFR GC */
uint32_t	ixfr_gc_interval = 86400;		/* How long between each IXFR GC */
uint32_t	ixfr_gc_delay=600;			/* After startup delay first GC by this much */
int		ignore_minimum = 0;			/* Ignore minimum TTL? */

int		forward_recursive = 0;			/* Forward recursive queries? */
int		recursion_timeout = 1;
int		recursion_connect_timeout = 1;
int		recursion_retries = 5;
const char	*recursion_algorithm = "linear";
char		*recursive_fwd_server = NULL;		/* Name of server for recursive forwarding */
int		recursive_family = AF_INET;		/* Protocol family for recursion */

int		wildcard_recursion = 0;			/* Search ancestor zones for wildcard matches - count give levels -1 means infinite */

const char	*mydns_dbengine = "MyISAM";

#if DEBUG_ENABLED
int		debug_enabled = 0;

int		debug_all = 0;

int		debug_alias = 0;
int		debug_array = 0;
int		debug_axfr = 0;
int		debug_cache = 0;
int		debug_conf = 0;
int		debug_data = 0;
int		debug_db = 0;
int		debug_encode = 0;
int		debug_error = 0;
int		debug_ixfr = 0;
int		debug_ixfr_sql = 0;
int		debug_lib_rr = 0;
int		debug_lib_soa = 0;
int		debug_listen = 0;
int		debug_memman = 0;
int		debug_notify = 0;
int		debug_notify_sql = 0;
int		debug_queue = 0;
int		debug_recursive = 0;
int		debug_reply = 0;
int		debug_resolve = 0;
int		debug_rr = 0;
int		debug_servercomms = 0;
int		debug_sort = 0;
int		debug_sql = 0;
int		debug_sql_queries = 0;
int		debug_status = 0;
int		debug_task = 0;
int		debug_tcp = 0;
int		debug_udp = 0;
int		debug_update = 0;
int		debug_update_sql = 0;
#endif

#if HAVE_IPV6
struct sockaddr_in6	recursive_sa6;			/* Recursive server (IPv6) */
#endif
struct sockaddr_in	recursive_sa;			/* Recursive server (IPv4) */

#ifdef DN_COLUMN_NAMES
char		*dn_default_ns = NULL;			/* Default NS for directNIC */
#endif

#define	V_(String)	((char*)(String))
/*
**  Default config values
**
**  If the 'name' is "-", the --dump-config option treats 'desc' as a header field.
*/
static CONF defConfig[] = {
/* name				value					desc										altname		defaulted	next	*/
  {	"-",			NULL,					N_("DATABASE INFORMATION"),							NULL,		0,		NULL	},
  {	"db-host",		V_("localhost"),			N_("SQL server hostname"),							NULL,		0,		NULL	},
  {	"db-user",		V_("username"),				N_("SQL server username"),							NULL,		0,		NULL	},
  {	"db-password",		V_("password"),				N_("SQL server password"),							NULL,		0,		NULL	},
  {	"database",		V_(PACKAGE_NAME),			N_("MyDNS database name"),							NULL,		0,		NULL	},

  {	"-",			NULL,					N_("GENERAL OPTIONS"),								NULL,		0,		NULL	},

  {	"user",			V_("nobody"),				N_("Run with the permissions of this user"),					NULL,		0,		NULL	},
  {	"group",		V_("nobody"),				N_("Run with the permissions of this group"),					NULL,		0,		NULL	},
  {	"listen",		V_("*"),				N_("Listen on these addresses ('*' for all)"),					"bind",		0,		NULL	},
  {	"no-listen",		V_(""),					N_("Do not listen on these addresses"),						NULL,		0,		NULL	},

  {	"-",			NULL,					N_("CACHE OPTIONS"),								NULL,		0,		NULL	},

  {	"cache-size",		V_("1024"),				N_("Maximum number of elements stored in the data/reply cache"),		NULL,		0,		NULL	},
  {	"cache-expire",		V_("60"),				N_("Number of seconds after which cached data/replies expire"),			NULL,		0,		NULL	},

  {	"zone-cache-size",	V_("1024"),				N_("Maximum number of elements stored in the zone cache"),			NULL,		0,		NULL	},
  {	"zone-cache-expire",	V_("60"),				N_("Number of seconds after which cached zones expires"),			NULL,		0,		NULL	},

  {	"reply-cache-size",	V_("1024"),				N_("Maximum number of elements stored in the reply cache"),			NULL,		0,		NULL	},
  {	"reply-cache-expire",	V_("30"),				N_("Number of seconds after which cached replies expire"),			NULL,		0,		NULL	},

  {	"-",			NULL,					N_("ESOTERICA"),								NULL,		0,		NULL	},
  {	"log",			V_("LOG_DAEMON"),			N_("Facility to use for program output (LOG_*/stdout/stderr)"),			NULL,		0,		NULL	},
  {	"pidfile",		V_("/var/run/"PACKAGE_NAME".pid"),	N_("Path to PID file"),								NULL,		0,		NULL	},
  {	"timeout",		V_("120"),				N_("Number of seconds after which queries time out"),				NULL,		0,		NULL	},
  {	"multicpu",		V_("-1"),				N_("Number of CPUs installed on your system - (deprecated)"),			NULL,		0,		NULL	},
  {	"servers",		V_("1"),				N_("Number of servers to run"),							NULL,		0,		NULL	},
  {	"recursive",		V_(""),					N_("Location of recursive resolver"),						NULL,		0,		NULL	},
  {	"recursive-timeout",	V_("1"),				N_("Number of seconds before first retry"),					NULL,		0,		NULL	},
  {	"recursive-retries",	V_("5"),				N_("Number of retries before abandoning recursion"),				NULL,		0,		NULL	},
  {	"recursive-algorithm",	V_("linear"),				N_("Recursion retry algorithm one of: linear, exponential, progressive"),	NULL,		0,		NULL	},
  {	"allow-axfr",		V_("no"),				N_("Should AXFR be enabled?"),							NULL,		0,		NULL	},
  {	"allow-tcp",		V_("no"),				N_("Should TCP be enabled?"),							NULL,		0,		NULL	},
  {	"allow-update",		V_("no"),				N_("Should DNS UPDATE be enabled?"),						NULL,		0,		NULL	},
  {	"ignore-minimum",	V_("no"),				N_("Ignore minimum TTL for zone?"),						NULL,		0,		NULL	},
  {	"soa-table",		V_(MYDNS_SOA_TABLE),			N_("Name of table containing SOA records"),					NULL,		0,		NULL	},
  {	"rr-table",		V_(MYDNS_RR_TABLE),			N_("Name of table containing RR data"),						NULL,		0,		NULL	},
  {	"use-soa-active",	V_("no"),				N_("Use the soa active attribute if provided"),					NULL,		0,		NULL	},
  {	"use-rr-active",	V_("no"),				N_("Use the rr active attribute if provided"),					NULL,		0,		NULL	},
  {	"notify-enabled",	V_("no"),				N_("Enable notify from updates"),						NULL,		0,		NULL	},
  {	"notify-source",	V_("0.0.0.0"),				N_("Source address for ipv4 notify messages"),					NULL,		0,		NULL	},
  {	"notify-source6",	V_("::"),				N_("Source address for ipv6 notify messages"),					NULL,		0,		NULL	},
  {	"notify-timeout",	V_("60"),				N_("Number of seconds before first retry"),					NULL,		0,		NULL	},
  {	"notify-retries",	V_("5"),				N_("Number of retries before abandoning notify"),				NULL,		0,		NULL	},
  {	"notify-algorithm",	V_("linear"),				N_("Notify retry algorithm one of: linear, exponential, progressive"),		NULL,		0,		NULL	},
  {	"ixfr-enabled",		V_("no"),				N_("Enable IXFR functionality"),						NULL,		0,		NULL	},
  {	"ixfr-gc-enabled",	V_("no"),				N_("Enable IXFR GC functionality"),						NULL,		0,		NULL	},
  {	"ixfr-gc-interval",	V_("86400"),				N_("How often to run GC for IXFR"),						NULL,		0,		NULL	},
  {	"ixfr-gc-delay",	V_("600"),				N_("Delay until first IXFR GC runs"),						NULL,		0,		NULL	},
  {	"extended-data-support",V_("no"),				N_("Support extended data fields for large TXT records"),			NULL,		0,		NULL	},
  {	"dbengine",		V_("MyISAM"),				N_("Support different database engines"),					NULL,		0,		NULL	},
  {	"wildcard-recursion",	V_("0"),				N_("Wildcard ancestor search levels"),						NULL,		0,		NULL	},

#ifdef DN_COLUMN_NAMES
  {	"default-ns",		V_("ns0.example.com."),			N_("Default nameserver for all zones"),						NULL,		0,		NULL	},
#endif

  {	"soa-where",		V_(""),					N_("Extra WHERE clause for SOA queries"),					NULL,		0,		NULL	},
  {	"rr-where",		V_(""),					N_("Extra WHERE clause for RR queries"),					NULL,		0,		NULL	},

#if DEBUG_ENABLED
  {	"debug-enabled",	V_("0"),				N_("Enable debug output from the config"),					NULL,		0,		NULL	},

  {	"debug-all",		V_("0"),				N_("Enable all debug output from the config"),					NULL,		0,		NULL	},

  {	"debug-alias",		V_("0"),				N_("Enable ALIAS code debugging"),						NULL,		0,		NULL	},
  {	"debug-array",		V_("0"),				N_("Enable ARRAY code debugging"),						NULL,		0,		NULL	},
  {	"debug-axfr",		V_("0"),				N_("Enable AXFR code debugging"),						NULL,		0,		NULL	},
  {	"debug-cache",		V_("0"),				N_("Enable CACHE code debugging"),						NULL,		0,		NULL	},
  {	"debug-conf",		V_("0"),				N_("Enable CONF code debugging"),						NULL,		0,		NULL	},
  {	"debug-data",		V_("0"),				N_("Enable DATA code debugging"),						NULL,		0,		NULL	},
  {	"debug-db",		V_("0"),				N_("Enable DB code debugging"),							NULL,		0,		NULL	},
  {	"debug-encode",		V_("0"),				N_("Enable ENCODE code debugging"),						NULL,		0,		NULL	},
  {	"debug-error",		V_("0"),				N_("Enable ERROR code debugging"),						NULL,		0,		NULL	},
  {	"debug-ixfr",		V_("0"),				N_("Enable IXFR code debugging"),						NULL,		0,		NULL	},
  {	"debug-ixfr-sql",	V_("0"),				N_("Enable IXFR SQL code debugging"),						NULL,		0,		NULL	},
  {	"debug-lib-rr",		V_("0"),				N_("Enable LIB/RR code debugging"),						NULL,		0,		NULL	},
  {	"debug-lib-soa",	V_("0"),				N_("Enable LIB/SOA code debugging"),						NULL,		0,		NULL	},
  {	"debug-listen",		V_("0"),				N_("Enable LISTEN code debugging"),						NULL,		0,		NULL	},
  {	"debug-memman",		V_("0"),				N_("Enable MEMMAN code debugging"),						NULL,		0,		NULL	},
  {	"debug-notify",		V_("0"),				N_("Enable NOTIFY code debugging"),						NULL,		0,		NULL	},
  {	"debug-notify-sql",	V_("0"),				N_("Enable NOTIFY SQL code debugging"),						NULL,		0,		NULL	},
  {	"debug-queue",		V_("0"),				N_("Enable QUEUE code debugging"),						NULL,		0,		NULL	},
  {	"debug-recursive",	V_("0"),				N_("Enable RECURSIVE code debugging"),						NULL,		0,		NULL	},
  {	"debug-reply",		V_("0"),				N_("Enable REPLY code debugging"),						NULL,		0,		NULL	},
  {	"debug-resolve",	V_("0"),				N_("Enable RESOLVE code debugging"),						NULL,		0,		NULL	},
  {	"debug-rr",		V_("0"),				N_("Enable RR code debugging"),							NULL,		0,		NULL	},
  {	"debug-servercomms",	V_("0"),				N_("Enable SERVERCOMMS code debugging"),					NULL,		0,		NULL	},
  {	"debug-sort",		V_("0"),				N_("Enable SORT code debugging"),						NULL,		0,		NULL	},
  {	"debug-sql",		V_("0"),				N_("Enable SQL code debugging"),						NULL,		0,		NULL	},
  {	"debug-sql-sueries",	V_("0"),				N_("Enable SQL QUERIES code debugging"),					NULL,		0,		NULL	},
  {	"debug-status",		V_("0"),				N_("Enable STATUS code debugging"),						NULL,		0,		NULL	},
  {	"debug-task",		V_("0"),				N_("Enable TASK code debugging"),						NULL,		0,		NULL	},
  {	"debug-tcp",		V_("0"),				N_("Enable TCP code debugging"),						NULL,		0,		NULL	},
  {	"debug-udp",		V_("0"),				N_("Enable UDP code debugging"),						NULL,		0,		NULL	},
  {	"debug-update",		V_("0"),				N_("Enable UPDATE code debugging"),						NULL,		0,		NULL	},
  {	"debug-update-sql",	V_("0"),				N_("Enable UPDATE SQL code debugging"),						NULL,		0,		NULL	},
#endif

  {	NULL,			NULL,				NULL,										NULL,		0,		NULL	}
};


/**************************************************************************************************
	DUMP_CONFIG
	Output configuration info (in a sort of config-file format).
**************************************************************************************************/
void
dump_config(void) {
  time_t	time_now = time(NULL);
  int		len = 0, w = 0, n, defaulted;
  char		pair[512], buf[80];
  CONF		*c;

  /*
  **	Pretty header
  */
  puts("##");
  puts("##  "MYDNS_CONF);
  printf("##  %.24s\n", ctime(&time_now));
  printf("##  %s\n", _("For more information, see mydns.conf(5)."));
  puts("##");

  /*
  ** Get longest words
  */
  for (n = 0; defConfig[n].name; n++) {
    const char *value = conf_get(&Conf, defConfig[n].name, &defaulted);

    c = &defConfig[n];
    if (!c->value || !c->value[0])
      continue;
    if (!value) {
      if ((len = strlen(c->name) + (c->value ? strlen(c->value) : 0)) > w)
	w = len;
    } else {
      char *cp, *vbuf, *v;
      if (!strcasecmp(c->name, "listen") || !strcasecmp(c->name, "no-listen")) {
	while ((cp = strchr(value, ',')))
	  *cp = CONF_FS_CHAR;
      }
      vbuf = STRDUP(value);
      for (cp = vbuf; (v = strsep(&cp, CONF_FS_STR));)
	if ((len = strlen(c->name) + strlen(v)) > w)
	  w = len;
      RELEASE(vbuf);
    }
  }
  w += strlen(" = ");

  /*
  **	Output name/value pairs
  */
  for (n = 0; defConfig[n].name; n++) {
    const char	*value = conf_get(&Conf, defConfig[n].name, &defaulted);

    c = &defConfig[n];

    if (c->name[0] == '-') {
      printf("\n\n%-*.*s\t# %s\n\n", w, w, " ", _(c->desc));
      continue;
    }

    if (!value) {
      if (!c->value || !c->value[0])
	continue;
      value = c->value;
      defaulted = 1;
    }

    /* Pick between "nobody" and "nogroup" for default group */
    if (!strcasecmp(c->name, "group") && getgrnam("nogroup"))
      c->value = V_("nogroup");

    /* If cache-size/cache-expire are set, copy values into data/reply-cache-size */
    if (!strcasecmp(c->name, "cache-size")) {
      if (defaulted) {
	continue;
      } else {
	snprintf(buf, sizeof(buf), "%d", atou(value) - (atou(value)/3));
	conf_clobber(&Conf, "zone-cache-size", buf);
	snprintf(buf, sizeof(buf), "%d", atou(value)/3);
	conf_clobber(&Conf, "reply-cache-size", buf);
      }
    } else if (!strcasecmp(c->name, "cache-expire")) {
      if (defaulted) {
	continue;
      } else {
	snprintf(buf, sizeof(buf), "%d", atou(value));
	conf_clobber(&Conf, "zone-cache-expire", buf);
	snprintf(buf, sizeof(buf), "%d", atou(value)/2);
	conf_clobber(&Conf, "reply-cache-expire", buf);
      }
    } else if (!strcasecmp(c->name, "listen") || !strcasecmp(c->name, "no-listen")) {
      char *cp, *vbuf, *v;
      while ((cp = strchr(value, ',')))
	*cp = CONF_FS_CHAR;
      vbuf = STRDUP(value);
      for (cp = vbuf; (v = strsep(&cp, CONF_FS_STR));) {
	if (v == vbuf) {
	  snprintf(pair, sizeof(pair), "%s = %s", c->name, v);
	  printf("%-*.*s\t# %s\n", w, w, pair, _(c->desc));
	} else {
	  printf("%s = %s\n", c->name, v);
	}
      }
      RELEASE(vbuf);
    } else {
      snprintf(pair, sizeof(pair), "%s = %s", c->name, value);
      printf("%-*.*s\t# %s\n", w, w, pair, _(c->desc));
    }
  }
  printf("\n");
}
/*--- dump_config() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	CONF_SET_LOGGING
	Sets the logging type and opens the syslog connection if necessary.
**************************************************************************************************/
void
conf_set_logging(void) {
  char *logtype;

  /* logtype is preset to LOG_DAEMON unless overridden */
  logtype = STRDUP(conf_get(&Conf, "log", NULL));
  strtolower(logtype);

  if (err_file) {
    if (err_file != stderr && err_file != stdout)
      fclose(err_file);
    err_file = NULL;
  } else {
    closelog();
  }

  if (!strcasecmp(logtype, "stderr")) { err_file = stderr; closelog(); }
  else if (!strcasecmp(logtype, "stdout")) { err_file = stdout; closelog(); }
  else if (!strcasecmp(logtype, "log_daemon")) error_init(NULL, LOG_DAEMON);
  else if (!strcasecmp(logtype, "log_local0")) error_init(NULL, LOG_LOCAL0);
  else if (!strcasecmp(logtype, "log_local1")) error_init(NULL, LOG_LOCAL1);
  else if (!strcasecmp(logtype, "log_local2")) error_init(NULL, LOG_LOCAL2);
  else if (!strcasecmp(logtype, "log_local3")) error_init(NULL, LOG_LOCAL3);
  else if (!strcasecmp(logtype, "log_local4")) error_init(NULL, LOG_LOCAL4);
  else if (!strcasecmp(logtype, "log_local5")) error_init(NULL, LOG_LOCAL5);
  else if (!strcasecmp(logtype, "log_local6")) error_init(NULL, LOG_LOCAL6);
  else if (!strcasecmp(logtype, "log_local7")) error_init(NULL, LOG_LOCAL7);
  else {
    FILE *fp;

    if (!(fp = fopen(logtype, "a")))
      Warn("%s: %s: %s", opt_conf, logtype, _("Error opening log file"));
    err_file = fp;
    closelog();
  }
  RELEASE(logtype);
}
/*--- conf_set_logging() ------------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_CONFIG_FILE_PERMS
**************************************************************************************************/
void
check_config_file_perms(void) {
  FILE *fp;

  if ((fp = fopen(opt_conf, "r"))) {
    Warnx("%s: %s", opt_conf, _("WARNING: config file is readable by unprivileged user"));
    fclose(fp);
  }
}
/*--- check_config_file_perms() -----------------------------------------------------------------*/


/**************************************************************************************************
	CONF_SET_RECURSIVE
	If the 'recursive' configuration option was specified, set the recursive server.
**************************************************************************************************/
static void
conf_set_recursive(void) {
  char		*c;
  const char	*address = conf_get(&Conf, "recursive", NULL);
  char		addr[512];
  int		port = 53;

  if (!address || !address[0])
    return;
  strncpy(addr, address, sizeof(addr)-1);

#if HAVE_IPV6
  if (is_ipv6(addr)) {		/* IPv6 - treat '+' as port separator */
    recursive_family = AF_INET6;
    if ((c = strchr(addr, '+'))) {
      *c++ = '\0';
      if (!(port = atoi(c)))
	port = 53;
    }
    if (inet_pton(AF_INET6, addr, &recursive_sa6.sin6_addr) <= 0) {
      Warnx("%s: %s", address, _("invalid network address for recursive server"));
      return;
    }
    recursive_sa6.sin6_family = AF_INET6;
    recursive_sa6.sin6_port = htons(port);
    forward_recursive = 1;
#if DEBUG_ENABLED && DEBUG_CONF
    DebugX("conf", 1,_("recursive forwarding service through %s:%u"),
	  ipaddr(AF_INET6, &recursive_sa6.sin6_addr), port);
#endif
    recursive_fwd_server = STRDUP(address);
  } else {			/* IPv4 - treat '+' or ':' as port separator  */
#endif
    recursive_family = AF_INET;
    if ((c = strchr(addr, '+')) || (c = strchr(addr, ':'))) {
      *c++ = '\0';
      if (!(port = atoi(c)))
	port = 53;
    }
    if (inet_pton(AF_INET, addr, &recursive_sa.sin_addr) <= 0) {
      Warnx("%s: %s", address, _("invalid network address for recursive server"));
      return;
    }
    recursive_sa.sin_family = AF_INET;
    recursive_sa.sin_port = htons(port);
#if DEBUG_ENABLED &&DEBUG_CONF
    DebugX("conf", 1,_("recursive forwarding service through %s:%u"),
	  ipaddr(AF_INET, &recursive_sa.sin_addr), port);
#endif
    forward_recursive = 1;
    recursive_fwd_server = STRDUP(address);
#if HAVE_IPV6
  }
#endif

  if (!forward_recursive) return;

  recursion_timeout = atou(conf_get(&Conf, "recursive-timeout", NULL));
  recursion_connect_timeout = atou(conf_get(&Conf, "recursive-connect-timeout", NULL));
  recursion_retries = atou(conf_get(&Conf, "recursive-retries", NULL));
  recursion_algorithm = conf_get(&Conf, "recursive-algorithm", NULL);

}
/*--- conf_set_recursive() ----------------------------------------------------------------------*/


/**************************************************************************************************
	LOAD_CONFIG
	Load the configuration file.
**************************************************************************************************/
void
load_config(void) {
  int		n;
  struct passwd *pwd = NULL;
  struct group	*grp = NULL;

  /* Load config */
  conf_load(&Conf, opt_conf);

  /* Set defaults */
  for (n = 0; defConfig[n].name; n++) {
    if (defConfig[n].name[0] == '-' || !defConfig[n].value)
      continue;
    if (!conf_get(&Conf, defConfig[n].name, NULL))
      conf_set(&Conf, defConfig[n].name, defConfig[n].value, 1);
  }

  /* Support "mysql-user" etc. for backwards compatibility */
  if (conf_get(&Conf, "mysql-host", NULL))
    conf_set(&Conf, "db-host", conf_get(&Conf, "mysql-host", NULL), 0);
  if (conf_get(&Conf, "mysql-user", NULL))
    conf_set(&Conf, "db-user", conf_get(&Conf, "mysql-user", NULL), 0);
  if (conf_get(&Conf, "mysql-pass", NULL))
    conf_set(&Conf, "db-password", conf_get(&Conf, "mysql-pass", NULL), 0);
  if (conf_get(&Conf, "mysql-password", NULL))
    conf_set(&Conf, "db-password", conf_get(&Conf, "mysql-password", NULL), 0);

#if HAVE_GETPWUID
  /* Set default for database username to real username if none was provided */
  if (!conf_get(&Conf, "db-user", NULL)) {
    struct passwd *pwd2;

    if ((pwd2 = getpwuid(getuid())) && pwd2->pw_name) {
      conf_set(&Conf, "db-user", pwd2->pw_name, 0);
      memset(pwd2, 0, sizeof(struct passwd));
    }
  }
#endif

  /* Load user/group perms */
  if (!(pwd = getpwnam(conf_get(&Conf, "user", NULL))))
    Err(_("error loading uid for user `%s'"), conf_get(&Conf, "user", NULL));
  perms_uid = pwd->pw_uid;
  perms_gid = pwd->pw_gid;
  memset(pwd, 0, sizeof(struct passwd));

  if (!(grp = getgrnam(conf_get(&Conf, "group", NULL))) && !(grp = getgrnam("nobody"))) {
    Warnx(_("error loading gid for group `%s'"), conf_get(&Conf, "group", NULL));
    Warnx(_("using gid %lu from user `%s'"), (unsigned long)perms_gid, conf_get(&Conf, "user", NULL));
  } else {
    perms_gid = grp->gr_gid;
    memset(grp, 0, sizeof(struct group));
  }

  /* We call conf_set_logging() again after moving into background, but it's called here
     to report on errors. */
  conf_set_logging();

  /* Set global options */
  task_timeout = atou(conf_get(&Conf, "timeout", NULL));

  axfr_enabled = GETBOOL(conf_get(&Conf, "allow-axfr", NULL));
  Verbose(_("AXFR is %senabled"), (axfr_enabled)?"":_("not "));

  tcp_enabled = GETBOOL(conf_get(&Conf, "allow-tcp", NULL));
  Verbose(_("TCP ports are %senabled"), (tcp_enabled)?"":_("not "));

  dns_update_enabled = GETBOOL(conf_get(&Conf, "allow-update", NULL));
  Verbose(_("DNS UPDATE is %senabled"), (dns_update_enabled)?"":_("not "));

  mydns_soa_use_active = GETBOOL(conf_get(&Conf, "use-soa-active", NULL));
  mydns_rr_use_active = GETBOOL(conf_get(&Conf, "use-rr-active", NULL));

  dns_notify_enabled = dns_update_enabled && GETBOOL(conf_get(&Conf, "notify-enabled", NULL));
  Verbose(_("DNS NOTIFY is %senabled"), (dns_notify_enabled)?"":_("not "));
  notify_timeout = atou(conf_get(&Conf, "notify-timeout", NULL));
  notify_retries = atou(conf_get(&Conf, "notify-retries", NULL));
  notify_algorithm = conf_get(&Conf, "notify-algorithm", NULL);

  dns_ixfr_enabled = GETBOOL(conf_get(&Conf, "ixfr-enabled", NULL));
  Verbose(_("DNS IXFR is %senabled"), (dns_ixfr_enabled)?"":_("not "));
  ixfr_gc_enabled = GETBOOL(conf_get(&Conf, "ixfr-gc-enabled", NULL));
  ixfr_gc_interval = atou(conf_get(&Conf, "ixfr-gc-interval", NULL));
  ixfr_gc_delay = atou(conf_get(&Conf, "ixfr-gc-delay", NULL));

  mydns_rr_extended_data = GETBOOL(conf_get(&Conf, "extended-data-support", NULL));

  mydns_dbengine = conf_get(&Conf, "dbengine", NULL);

  wildcard_recursion = atoi(conf_get(&Conf, "wildcard-recursion", NULL));

  ignore_minimum = GETBOOL(conf_get(&Conf, "ignore-minimum", NULL));

  /* Set table names if provided */
  mydns_set_soa_table_name(conf_get(&Conf, "soa-table", NULL));
  mydns_set_rr_table_name(conf_get(&Conf, "rr-table", NULL));

  /* Set additional where clauses if provided */
  mydns_set_soa_where_clause(conf_get(&Conf, "soa-where", NULL));
  mydns_set_rr_where_clause(conf_get(&Conf, "rr-where", NULL));

  /* Set recursive server if specified */
  conf_set_recursive();

#ifdef DN_COLUMN_NAMES
  dn_default_ns = conf_get(&Conf, "default-ns", NULL);
#endif
}
/*--- load_config() -----------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
