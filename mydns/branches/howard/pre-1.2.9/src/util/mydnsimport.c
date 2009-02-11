/**************************************************************************************************
	$Id: import.c,v 1.27 2005/04/20 17:22:25 bboy Exp $

	import.c: Import DNS data.

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

#include "named.h"
#include "util.h"

#include "debug.h"
#include "import.h"
#include "rr.h"

#define DEFINE_RR_MAP 1

#define DEFINE_RR_TYPE(__TYPENAME__, __PERSISTENT__, __PARSER__, __FREE__, __DUPLICATOR__, \
		       __SIZOR__, __REPLY_ADD__, __UPDATE_GET_RR_DATA__, __PROCESS_AXFR__, \
		       __CHECK_RR__, \
		       __EXPORT_BIND_RR__, __EXPORT_TINYDNS_RR__, \
		       __UPDATE_ENABLED__,				\
		       __MATCH_AUX__, __WHERECLAUSE__)			\
  static dns_qtype_map __RR_TYPE_##__TYPENAME__ = { "" #__TYPENAME__ "", \
						    DNS_QTYPE_##__TYPENAME__, \
						    __PERSISTENT__, \
						    __PARSER__, \
						    __FREE__, \
						    __DUPLICATOR__, \
						    __SIZOR__, \
						    NULL, \
						    NULL, \
						    __PROCESS_AXFR__, \
						    NULL, \
						    NULL, \
						    NULL, \
						    __UPDATE_ENABLED__, \
						    __MATCH_AUX__, \
						    __WHERECLAUSE__ }

#include "rrtype.h"

#if DEBUG_ENABLED
int		debug_mydnsimport = 0;

debugging_switch debugging_switches[] = {
  {	"all",			&debug_all },

  {	"import",		&debug_import },
  {	"util",			&debug_util },

  {	"mydnsimport",		&debug_mydnsimport },

  {	NULL,			NULL }

};

#endif

enum _input_format {						/* Import format types */
	INPUT_UNKNOWN,
	INPUT_AXFR,
	INPUT_TINYDNS
} input_format = INPUT_UNKNOWN;

char *axfr_host = NULL;						/* Host to import via AXFR */
char *tinydns_datafile = NULL;					/* Name of tinydns-data format file */

/**************************************************************************************************
	USAGE
	Display program usage information.
**************************************************************************************************/
static void
usage(int status, char *unrecognized_option) {
  if (status != EXIT_SUCCESS) {
    if (unrecognized_option) {
      fprintf(stderr, _("unrecognized option '%s'\n"), unrecognized_option);
    }
    fprintf(stderr, _("Try `%s --help' for more information."), progname);
    fputs("\n", stderr);
  } else {
    printf(_("Usage: %s [OPTION]... [ZONE]..."), progname);
    puts("");
    puts(_("Import DNS data."));
    puts("");
    puts(_("  -c, --conf=FILE         read config from FILE instead of the default"));
    puts(_("  -a, --axfr=HOST         import zones from HOST via AXFR"));
    puts(_("  -t, --tinydns=FILE      import zones from tinydns-data format FILE"));
    puts("");
    puts(_("  -D, --database=DB       database name to use"));
    puts(_("  -h, --host=HOST         connect to SQL server at HOST"));
    puts(_("  -p, --password=PASS     password for SQL server (or prompt from tty)"));
    puts(_("  -u, --user=USER         username for SQL server if not current user"));
    puts("");
    puts(_("  -o, --output            output data instead of inserting records"));
    puts(_("  -r, --replace           if ZONE already exists, replace it"));
    puts(_("      --notrim            do not remove trailing origin from names"));
    puts(_("  -I  --IXFR              support IXFR by adding serial number to rr records"));
    puts(_("  -A  --ACTIVE=active     support active field by adding to records"));
    puts("");
#if DEBUG_ENABLED
    puts(_("  -d, --debug             enable debug output"));
#endif
    puts(_("  -v, --verbose           be more verbose while running"));
    puts(_("      --help              display this help and exit"));
    puts(_("      --version           output version information and exit"));
    puts("");
    puts(_("If no ZONE is specified, all zones found will be imported.  If AXFR"));
    puts(_("is being used as the import method, the zone names must be specified."));
    puts("");
    printf(_("Report bugs to <%s>.\n"), PACKAGE_BUGREPORT);
  }
  exit(status);
}
/*--- usage() -----------------------------------------------------------------------------------*/


/**************************************************************************************************
	CMDLINE
	Process command line options.
**************************************************************************************************/
static void
cmdline(int argc, char **argv) {
  char	*optstr;
  int	optc, optindex;
  struct option const longopts[] = {
    {"axfr",			required_argument,	NULL,	'a'},
    {"tinydns",			required_argument,	NULL,	't'},

    {"conf",			required_argument,	NULL,	'c'},
    {"database",		required_argument,	NULL,	'D'},
    {"host",			required_argument,	NULL,	'h'},
    {"password",		optional_argument,	NULL,	'p'},
    {"user",			required_argument,	NULL,	'u'},

    {"output",			no_argument,		NULL,	'o'},
    {"replace",			no_argument,		NULL,	'r'},

    {"notrim",			no_argument,		NULL,	0},

    {"IXFR",			no_argument,		NULL,	'I'},
    {"ACTIVE",			optional_argument,	NULL,	'A'},

    {"debug",			no_argument,		NULL,	'd'},
    {"verbose",			no_argument,		NULL,	'v'},

    {"help",			no_argument,		NULL,	0},
    {"version",			no_argument,		NULL,	0},

    {NULL,				0,			NULL,	0}
  };

  err_file = stdout;

  error_init(argv[0], LOG_USER);					/* Init output routines */

  optstr = getoptstr(longopts);
  opterr = 0;

  while ((optc = getopt_long(argc, argv, optstr, longopts, &optindex)) != -1) {
    switch (optc) {
    case 0:
      {
	const char *opt = longopts[optindex].name;
	if (!strcmp(opt, "version")) {					/* --version */
	  printf("%s ("PACKAGE_NAME") "PACKAGE_VERSION" ("SQL_VERSION_STR")\n", progname);
	  puts("\n" PACKAGE_COPYRIGHT);
	  puts(_("This is free software; see the source for copying conditions.  There is NO"));
	  puts(_("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."));
	  exit(EXIT_SUCCESS);
	} else if (!strcmp(opt, "help"))				/* --help */
	  usage(EXIT_SUCCESS, NULL);
	else if (!strcmp(opt, "notrim"))				/* --notrim */
	  opt_notrim = 1;
      }
      break;
    case 'a':								/* -a, --axfr */
      axfr_host = optarg;
      input_format = INPUT_AXFR;
      break;
    case 'A':								/* -A, --ACTIVE */
      if (optarg) {
	ACTIVE = optarg;
      } else {
	ACTIVE=(char*)"Y";
      }
      break;
    case 't':								/* -t, --tinydns */
      tinydns_datafile = optarg;
      input_format = INPUT_TINYDNS;
      break;
    case 'c':								/* -c, --conf=FILE */
      opt_conf = optarg;
      break;

    case 'd':								/* -d, --debug */
#if DEBUG_ENABLED
      err_verbose = 9;
      debug_set("enabled", "9");
      debug_set("all", "9");
#endif
      break;
    case 'D':								/* -D, --database=DB */
      conf_set(&Conf, "database", optarg, 0);
      break;
    case 'h':								/* -h, --host=HOST */
      conf_set(&Conf, "db-host", optarg, 0);
      break;
    case 'I':								/* -I, --IXFR */
      IXFR = 1;
      if (!ACTIVE) ACTIVE=(char*)"Y";
      break;
    case 'p':								/* -p, --password=PASS */
      if (optarg) {
	conf_set(&Conf, "db-password", optarg, 0);
	memset(optarg, 'X', strlen(optarg));
      } else
	conf_set(&Conf, "db-password", passinput(_("Enter password")), 0);
      break;
    case 'u':								/* -u, --user=USER */
      conf_set(&Conf, "db-user", optarg, 0);
      break;

    case 'o':								/* -o, --output */
      opt_output = 1;
      break;
    case 'r':								/* -r, --replace */
      opt_replace = 1;
      break;
    case 'v':								/* -v, --verbose */
      err_verbose = 1;
      break;

    case '?':
#if DEBUG_ENABLED
      if (debug_parse(argc, argv)) {
	break;
      }
#endif

    default:
      usage(EXIT_FAILURE, argv[optind-1]);
    }
  }
}
/*--- cmdline() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	MAIN
**************************************************************************************************/
int
main(int argc, char **argv) {
#if DEBUG_ENABLED
  debug_start(debugging_switches);
#endif
  SET_TYPEMAPS();
  setlocale(LC_ALL, "");				/* Internationalization */
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
  cmdline(argc, argv);
  load_config();

  rr_imported = ptr_imported = 0;

  if (input_format == INPUT_UNKNOWN) {			/* No input format specified */
    Warnx(_("no input format specified"));
    usage(EXIT_FAILURE, NULL);
  }

  /* If we're inserting directly into the database, connect to the database */
  if (!opt_output)
    db_connect();

  if (optind >= argc) {					/* No arguments - try to import all zones */
    switch (input_format) {
    case INPUT_AXFR:
      Warnx(_("using AXFR method; no zones specified"));
      usage(EXIT_FAILURE, NULL);
      break;

    case INPUT_TINYDNS:
      import_tinydns(tinydns_datafile, NULL);
      break;

    case INPUT_UNKNOWN:
      break;
    }
  }

  while (optind < argc) {
    switch (input_format) {
    case INPUT_AXFR:
      import_axfr(axfr_host, (char *)argv[optind]);
      if (import_zone_id)
	Verbose("%s: zone %u (%u soa / %u rr / %u ptr)",
		axfr_host, import_zone_id, soa_imported,
		rr_imported, ptr_imported);
      break;

    case INPUT_TINYDNS:
      import_tinydns(tinydns_datafile, (char *)argv[optind]);
      break;

    case INPUT_UNKNOWN:
      break;
    }
    optind++;
  }

  return (0);
}
/*--- main() ------------------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
