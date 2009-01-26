/**********************************************************************************************
	$Id: check.c,v 1.36 2005/05/04 16:49:59 bboy Exp $

	check.c: Check for problems with the data in the database.

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
**********************************************************************************************/

#include "named.h"
#include "util.h"

#include "memoryman.h"
#include "check.h"
#include "debug.h"

MYDNS_SOA	*soa;					/* Current SOA record being scanned */
MYDNS_RR	*rr;					/* Current RR record */
char		*name = NULL;				/* Current expanded name */
char		*data = NULL;				/* Current expanded data */
int		opt_consistency = 0;			/* Consistency check? */
int		opt_consistency_only = 0;		/* Consistency check only? */


int		syntax_errors, consistency_errors;	/* Number of errors found */

/**********************************************************************************************
	USAGE
	Display program usage information.
**********************************************************************************************/
static void
usage(int status, char *unrecognized_option) {
  if (status != EXIT_SUCCESS) {
    if (unrecognized_option) {
      fprintf(stderr, _("unrecognized option '%s'\n"), unrecognized_option);
    }
    fprintf(stderr, _("Try `%s --help' for more information."), progname);
    fputs("\n", stderr);
  } else {
    printf(_("Usage: %s [ZONE..]"), progname);
    puts("");
    puts(_("Check zone(s) or entire database for errors and consistency."));
    puts("");
    puts(_("  -c, --conf=FILE         read config from FILE instead of the default"));
    puts(_("  -c, --consistency       do key consistency checks"));
    puts(_("  -C, --consistency-only  do only the key consistency checks"));
    puts(_("  -x, --extended          extended check for data/name references"));
    puts("");
    puts(_("  -D, --database=DB       database name to use"));
    puts(_("  -h, --host=HOST         connect to SQL server at HOST"));
    puts(_("  -p, --password=PASS     password for SQL server (or prompt from tty)"));
    puts(_("  -u, --user=USER         username for SQL server if not current user"));
    puts("");
#if DEBUG_ENABLED
    puts(_("  -d, --debug             enable debug output"));
#endif
    puts(_("  -v, --verbose           be more verbose while running"));
    puts(_("      --help              display this help and exit"));
    puts(_("      --version           output version information and exit"));
    puts("");
    printf(_("Report bugs to <%s>.\n"), PACKAGE_BUGREPORT);
  }
  exit(status);
}
/*--- usage() -------------------------------------------------------------------------------*/


/**********************************************************************************************
	CMDLINE
	Process command line options.
**********************************************************************************************/
static void
cmdline(int argc, char **argv) {
  char	*optstr;
  int	optc, optindex;
  struct option const longopts[] = {
    {"conf",			required_argument,	NULL,	'f'},
    {"consistency-only",	no_argument,		NULL,	'C'},
    {"consistency",		no_argument,		NULL,	'c'},
    {"extended",		no_argument,		NULL,	'x'},

    {"database",		required_argument,	NULL,	'D'},
    {"host",			required_argument,	NULL,	'h'},
    {"password",		optional_argument,	NULL,	'p'},
    {"user",			required_argument,	NULL,	'u'},

    {"debug",			no_argument,		NULL,	'd'},
    {"verbose",			no_argument,		NULL,	'v'},
    {"help",			no_argument,		NULL,	0},
    {"version",			no_argument,		NULL,	0},

    {NULL,			0,			NULL,	0}
  };

  err_file = stdout;

  error_init(argv[0], LOG_USER);				/* Init output routines */

  optstr = getoptstr(longopts);
  opterr = 0;

  while ((optc = getopt_long(argc, argv, optstr, longopts, &optindex)) != -1) {
    switch (optc) {
    case 0:
      {
	const char *opt = longopts[optindex].name;

	if (!strcmp(opt, "version")) {				/* --version */
	  printf("%s ("PACKAGE_NAME") "PACKAGE_VERSION" ("SQL_VERSION_STR")\n", progname);
	  puts("\n" PACKAGE_COPYRIGHT);
	  puts(_("This is free software; see the source for copying conditions.  There is NO"));
	  puts(_("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."));
	  exit(EXIT_SUCCESS);
	} else if (!strcmp(opt, "help"))			/* --help */
	  usage(EXIT_SUCCESS, NULL);
      }
      break;
    case 'C':							/* -C, --consistency-only */
      opt_consistency = opt_consistency_only = 1;
      break;
    case 'c':							/* -c, --consistency */
      opt_consistency = 1;
      break;
    case 'd':							/* -d, --debug */
#if DEBUG_ENABLED
      err_verbose = err_debug = 1;
#endif
      break;
    case 'D':							/* -D, --database=DB */
      conf_set(&Conf, "database", optarg, 0);
      break;
    case 'f':							/* -f, --conf=FILE */
      opt_conf = optarg;
      break;
    case 'h':							/* -h, --host=HOST */
      conf_set(&Conf, "db-host", optarg, 0);
      break;
    case 'p':							/* -p, --password=PASS */
      if (optarg) {
	conf_set(&Conf, "db-password", optarg, 0);
	memset(optarg, 'X', strlen(optarg));
      }	else
	conf_set(&Conf, "db-password", passinput(_("Enter password")), 0);
      break;
    case 'u':							/* -u, --user=USER */
      conf_set(&Conf, "db-user", optarg, 0);
      break;

    case 'v':							/* -v, --verbose */
      err_verbose = 1;
      break;
    case 'x':							/* -x, --extended */
      opt_extended_check = 1;
      break;
    case '?':
#if DEBUG_ENABLED
      if (debug_parse(argc, argv)) {
	break;
      }
#endif

    default:
      usage(EXIT_FAILURE, argv[optind - 1]);
    }
  }
  load_config();
}
/*--- cmdline() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_SOA
	Perform SOA check for this zone and return the SOA record.
	Checks currently performed:
		- Make sure "ns" and "mbox" are present and valid.
		- Make sure none of the numeric values are unreasonable (like 0)
**************************************************************************************************/
static MYDNS_SOA *
check_soa(const char *zone) {
  if (mydns_soa_load(sql, &soa, (char *)zone) != 0)
    Errx("%s: %s", zone, _("error loading SOA record for zone"));
  if (!soa)
    Errx("%s: %s", zone, _("zone not found"));
  rr = NULL;

  /* SOA validation */
  name = REALLOCATE(name, strlen(soa->origin) + 1, char[]);
  strcpy(name, soa->origin);
  mydns_check_name(soa, rr, name, data, soa->ns, strlen(soa->ns), "soa.ns", 0, 0);
  mydns_check_name(soa, rr, name, data, soa->mbox, strlen(soa->mbox), "soa.mbox", 0, 0);

  if (LASTCHAR(name) != '.')
    mydns_rrproblem(soa, rr, name, data, _("soa.origin is not a FQDN (no trailing dot)"));

  if (soa->refresh < 300) mydns_rrproblem(soa, rr, name, data, _("soa.refresh is less than 300 seconds"));
  if (soa->retry < 300) mydns_rrproblem(soa, rr, name, data, _("soa.retry is less than 300 seconds"));
  if (soa->expire < 300) mydns_rrproblem(soa, rr, name, data, _("soa.expire is less than 300 seconds"));
  if (soa->minimum < 300) mydns_rrproblem(soa, rr, name, data, _("soa.minimum is less than 300 seconds"));
  if (soa->ttl < 300) mydns_rrproblem(soa, rr, name, data, _("soa.ttl is less than 300 seconds"));
  if (soa->minimum < 300) mydns_rrproblem(soa, rr, name, data, _("soa.minimum is less than 300 seconds"));

  return (soa);
}
/*--- check_soa() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_RR
	Check an individual resource record.
**************************************************************************************************/
static void
check_rr(void) {
  /* Expand RR's name into `name' */
  int	namelen = strlen(MYDNS_RR_NAME(rr));
  dns_qtype_map *map;

  name = REALLOCATE(name, namelen+1, char[]);
  memset(name, 0, namelen+1);
  data = REALLOCATE(data, MYDNS_RR_DATA_LENGTH(rr)+1, char[]);
  memset(data, 0, MYDNS_RR_DATA_LENGTH(rr)+1);
  strncpy(name, MYDNS_RR_NAME(rr), namelen);
  memcpy(data, MYDNS_RR_DATA_VALUE(rr), MYDNS_RR_DATA_LENGTH(rr));
  name = mydns_expand_data(name, soa->origin);

  map = mydns_rr_get_type_by_id(rr->type);

  if (!ignore_minimum && (rr->ttl < soa->minimum))
    mydns_rrproblem(soa, rr, name, data, _("TTL below zone minimum"));

  map->rr_check_rr(soa, rr, name, data, data, MYDNS_RR_DATA_LENGTH(rr));

}
/*--- check_rr() --------------------------------------------------------------------------------*/

                                                                                                                               

/**************************************************************************************************
	CHECK_ZONE
	Checks each RR in the current zone through check_rr.
**************************************************************************************************/
static void
check_zone(void) {
  char *query;
  int querylen;
  unsigned int rrct = 0;
  SQL_RES *res;
  SQL_ROW row;
  unsigned long *lengths;
  char * columns = NULL;

  columns = mydns_rr_columns();

  query = mydns_rr_prepare_query(soa->id, DNS_QTYPE_ANY, NULL, soa->origin, mydns_rr_active_types[0],
				 columns, NULL);
  RELEASE(columns);

#ifdef notdef
  querylen = ASPRINTF(&query, "SELECT "MYDNS_RR_FIELDS" FROM %s WHERE zone=%u",
		      mydns_rr_table_name, soa->id);
#endif
  res = sql_query(sql, query, strlen(query));
  RELEASE(query);
  if (!res) 
    return;

  while ((row = sql_getrow(res, &lengths))) {
    if (!(rr = mydns_rr_parse(row, lengths, soa->origin)))
      continue;
    check_rr();
    mydns_rr_free(rr);
    rrct++;
  }
  sql_free(res);

  if (err_verbose) {
    meter(0, 0);
    Verbose("%s: %u %s", soa->origin, rrct, rrct == 1 ? _("resource record") : _("resource records"));
  }
}
/*--- check_zone() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	CONSISTENCY_RR_ZONE
	Makes sure rr.zone matches a soa.id.
**************************************************************************************************/
static void
consistency_rr_zone(void) {
  char *query;
  size_t querylen;
  SQL_RES *res;
  SQL_ROW row;

  querylen = ASPRINTF(&query,
		      "SELECT %s.id,%s.zone FROM %s LEFT JOIN %s ON %s.zone=%s.id WHERE %s.id IS NULL",
		      mydns_rr_table_name, mydns_rr_table_name, mydns_rr_table_name,
		      mydns_soa_table_name, mydns_rr_table_name, mydns_soa_table_name,
		      mydns_soa_table_name);
  res = sql_query(sql, query, querylen);
  RELEASE(query);
  if (!res)
    return;
  while ((row = sql_getrow(res, NULL))) {
    char *msg = NULL;

    meter(0,0);
    ASPRINTF(&msg,
	     _("%s id %s references invalid %s id %s"),
	     mydns_rr_table_name, row[0], mydns_soa_table_name, row[1]);
    printf("%s\t-\t%s\t-\t-\t-\t-\t-\n", msg, row[0]);
    RELEASE(msg);
    fflush(stdout);

    consistency_errors++;
  }
  sql_free(res);
}
/*--- consistency_rr_zone() ---------------------------------------------------------------------*/


/**************************************************************************************************
	CONSISTENCY_CHECK
	Does a general database consistency check - makes sure all keys are kosher.
**************************************************************************************************/
static void
consistency_check(void) {
  consistency_rr_zone();
}
/*--- consistency_check() -----------------------------------------------------------------------*/


/**************************************************************************************************
	MAIN
**************************************************************************************************/
int
main(int argc, char **argv) {
  setlocale(LC_ALL, "");					/* Internationalization */
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
  cmdline(argc, argv);

  db_connect();

  db_check_optional();

  if (!opt_consistency_only) {
    if (optind >= argc)	{					/* Check all zones */
      char *query;
      size_t querylen;
      SQL_RES *res;
      SQL_ROW row;
      unsigned long current = 0, total;

      querylen = ASPRINTF(&query, "SELECT origin FROM %s", mydns_soa_table_name);
      res = sql_query(sql, query, querylen);
      RELEASE(query);
      if(res) {
	total = sql_num_rows(res);
	while ((row = sql_getrow(res, NULL))) {
	  meter(current++, total);
	  if ((soa = check_soa(row[0]))) {
	    check_zone();
	    mydns_soa_free(soa);
	  }
	}
	sql_free(res);
      }
    }
    else while (optind < argc) {				/* Check zones provided as args */
      char *zone;
      int zonelen = strlen(argv[optind]) + 1;
      if (*argv[optind] && LASTCHAR(argv[optind]) != '.') zonelen += 1;
      zone = ALLOCATE(zonelen, char[]);
      strcpy(zone, argv[optind++]);
      if (*zone && LASTCHAR(zone) != '.')
	strcat(zone, ".");
      if ((soa = check_soa(zone))) {
	check_zone();
	mydns_soa_free(soa);
      }
      RELEASE(zone);
    }
  }

  if (opt_consistency)
    consistency_check();					/* Do consistency check if requested */
  
  meter(0, 0);
  if (!syntax_errors && !consistency_errors)
    Verbose(_("No errors"));
  else {
    if (opt_consistency_only)
      Verbose("%s: %d", _("Consistency errors"), consistency_errors);
    else if (opt_consistency)
      Verbose("%s: %d  %s: %d",
	      _("Syntax errors"), syntax_errors, _("Consistency errors"), consistency_errors);
    else
      Verbose("%s: %d", _("Syntax errors"), syntax_errors);
  }

  return (0);
}
/*--- main() ------------------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
