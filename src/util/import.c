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

#include "util.h"

#define DEBUG_SQL	0

int opt_output = 0;						/* Output instead of insert */
int opt_notrim = 0;						/* Don't remove trailing origin */
int opt_replace = 0;						/* Replace current zone */
int IXFR = 0;							/* Serial number on the rr records */
const char *ACTIVE = NULL;						/* ACTIVE String to use */

enum _input_format {						/* Import format types */
	INPUT_UNKNOWN,
	INPUT_AXFR,
#ifdef TINYDNS_IMPORT
	INPUT_TINYDNS
#endif
} input_format = INPUT_UNKNOWN;

char *axfr_host = NULL;						/* Host to import via AXFR */
#ifdef TINYDNS_IMPORT
char *tinydns_datafile = NULL;					/* Name of tinydns-data format file */
#endif

uint32_t	import_zone_id = 0;				/* ID of current zone */
uint32_t	import_serial = 0;				/* Serial number of current zone */
static int	soa_imported, rr_imported, ptr_imported;	/* Number of records imported */


/**************************************************************************************************
	USAGE
	Display program usage information.
**************************************************************************************************/
static void
usage(int status) {
  if (status != EXIT_SUCCESS) {
    fprintf(stderr, _("Try `%s --help' for more information."), progname);
    fputs("\n", stderr);
  } else {
    printf(_("Usage: %s [OPTION]... [ZONE]..."), progname);
    puts("");
    puts(_("Import DNS data."));
    puts("");
    puts(_("  -c, --conf=FILE         read config from FILE instead of the default"));
    puts(_("  -a, --axfr=HOST         import zones from HOST via AXFR"));
#ifdef TINYDNS_IMPORT
    puts(_("  -t, --tinydns=FILE      import zones from tinydns-data format FILE"));
#endif
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
#ifdef TINYDNS_IMPORT
    {"tinydns",			required_argument,	NULL,	't'},
#endif

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
	  usage(EXIT_SUCCESS);
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
	ACTIVE="Y";
      }
      break;
#ifdef TINYDNS_IMPORT
    case 't':								/* -t, --tinydns */
      tinydns_datafile = optarg;
      input_format = INPUT_TINYDNS;
      break;
#endif
    case 'c':								/* -c, --conf=FILE */
      opt_conf = optarg;
      break;

    case 'd':								/* -d, --debug */
#if DEBUG_ENABLED
      err_verbose = err_debug = 1;
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
      if (!ACTIVE) ACTIVE="Y";
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
    default:
      usage(EXIT_FAILURE);
    }
  }
}
/*--- cmdline() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	IMPORT_SOA
	Update, replace, or output SOA.  Returns the zone ID or 1 if not working with database.
**************************************************************************************************/
uint32_t
import_soa(const uchar *import_origin, const uchar *ns, const uchar *mbox,
	   unsigned serial, unsigned refresh, unsigned retry, unsigned expire,
	   unsigned minimum, unsigned ttl) {
  char *esc_origin, *esc_ns, *esc_mbox;

  soa_imported++;
  import_zone_id = 0;
  import_serial = serial;

  Verbose("import soa \"%s\"", import_origin);

  if (opt_output) {							/* Not updating database */
    printf("soa\t%s\t%s\t%s\t%u\t%u\t%u\t%u\t%u\t%u\n",
	   import_origin, ns, mbox, serial, refresh, retry, expire, minimum, ttl);
    return (1);
  }

  /* SQL-escape the strings */
  esc_origin = sql_escstr(sql, (char *)import_origin);
  esc_ns = sql_escstr(sql, (char *)ns);
  esc_mbox = sql_escstr(sql, (char *)mbox);

  /* If this zone already exists, update the "soa" record and delete all "rr" record(s) */
  import_zone_id = sql_count(sql, "SELECT id FROM %s WHERE origin='%s'",
			     mydns_soa_table_name, esc_origin);

  if (import_zone_id > 0) {
    if (!opt_replace) {
      Warnx("%s: %s", import_origin, _("zone already exists"));
      Errx(_("use -r (--replace) to overwrite existing zone"));
    }
    /* Delete from "rr" table */
#if DEBUG_SQL
    Debug("DELETE FROM %s WHERE zone=%u;", mydns_rr_table_name, import_zone_id);
#endif
    sql_queryf(sql, "DELETE FROM %s WHERE zone=%u;", mydns_rr_table_name, import_zone_id);

    /* Update "soa" table */
#if DEBUG_SQL
    Debug("UPDATE %s SET origin='%s',ns='%s',mbox='%s',serial=%u,refresh=%u,retry=%u,"
	  "expire=%u,minimum=%u,ttl=%u WHERE id=%u;", mydns_soa_table_name,
	  esc_origin, esc_ns, esc_mbox, serial, refresh, retry, expire, minimum, ttl,
	  import_zone_id);
#endif
    sql_queryf(sql,
	       "UPDATE %s SET origin='%s',ns='%s',mbox='%s',serial=%u,refresh=%u,retry=%u,"
	       "expire=%u,minimum=%u,ttl=%u WHERE id=%u;", mydns_soa_table_name,
	       esc_origin, esc_ns, esc_mbox, serial, refresh, retry, expire, minimum, ttl,
	       import_zone_id);
  } else {
    /* Otherwise, just create the new zone */
    Verbose("origin: [%s]", esc_origin);
    Verbose("ns: [%s]", esc_ns);
    Verbose("mbox: [%s]", esc_mbox);
#if DEBUG_SQL
    Debug("INSERT INTO %s (origin,ns,mbox,serial,refresh,retry,expire,minimum,ttl)"
	  " VALUES ('%s','%s','%s',%u,%u,%u,%u,%u,%u);",
	  mydns_soa_table_name,
	  esc_origin, esc_ns, esc_mbox, serial, refresh, retry, expire, minimum, ttl);
#endif
    sql_queryf(sql,
	       "INSERT INTO %s (origin,ns,mbox,serial,refresh,retry,expire,minimum,ttl)"
	       " VALUES ('%s','%s','%s',%u,%u,%u,%u,%u,%u);",
	       mydns_soa_table_name,
	       esc_origin, esc_ns, esc_mbox, serial, refresh, retry, expire, minimum, ttl);

    /* Store SOA for new record in `got_soa' */	
    if ((import_zone_id = sql_count(sql, "SELECT id FROM %s WHERE origin='%s'",
				    mydns_soa_table_name, esc_origin)) < 1)
      Errx(_("error getting id for new soa record"));
  }
  RELEASE(esc_origin);
  RELEASE(esc_ns);
  RELEASE(esc_mbox);
  return (import_zone_id);
}
/*--- import_soa() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	IMPORT_RR
	Update/output results for an RR record.
**************************************************************************************************/
void
import_rr(const uchar *name, const uchar *type, const uchar *data,
	  size_t datalen, unsigned aux, unsigned ttl) {
  char *esc_name, *esc_data, *esc_edata = NULL;
  const char *querystr = NULL;
  const uchar *edata = NULL;
  size_t edatalen = 0;


  if (!name || !type || !data)
    Errx(_("Invalid resource record name=%s, type=%s, data=%p"),
	 (name) ? (char*)name : "<undef>", (type) ? (char*)type : "<undef>", data);

  rr_imported++;
  Verbose("import rr \"%s\" %s \"%s\"", name, type, data);

  if (opt_output) {
    printf("rr\t%s\t%s\t%s\t%u\t%u\n", name, type, data, aux, ttl);
    return;
  }
  if (import_zone_id < 1)
    Errx(_("got rr record without soa"));

  /* s = shortname(name); */
  esc_name = sql_escstr(sql, (char*)name);
  if (mydns_rr_extended_data && datalen > mydns_rr_data_length) {
    edatalen = datalen - mydns_rr_data_length;
    edata = &data[mydns_rr_data_length];
    datalen = mydns_rr_data_length;
    esc_edata = sql_escstr2(sql, (char*)edata, edatalen);
  }
  esc_data = sql_escstr2(sql, (char*)data, datalen);
  if (IXFR) {
    querystr = "INSERT INTO %s (zone,name,type,data%s,aux,ttl,active,serial) "
      "VALUES (%u,'%s','%s','%s'%s%s%s,%u,%u,'%s',%u);";
#if DEBUG_SQL
    Debug(querystr, mydns_rr_table_name, (edatalen)?",edata":"",
	  import_zone_id, esc_name, type, esc_data,
	  (edatalen)?",'":"",
	  (edatalen)?esc_edata:"",
	  (edatalen)?"'":""
	  aux, ttl, ACTIVE, import_serial);
#endif
    sql_queryf(sql,
	       querystr, mydns_rr_table_name, (edatalen)?",edata":"",
	       import_zone_id, esc_name, type, esc_data,
	       (edatalen)?",'":"",
	       (edatalen)?esc_edata:"",
	       (edatalen)?"'":"",
	       aux, ttl, ACTIVE, import_serial);
  } else if (ACTIVE) {
    querystr = "INSERT INTO %s (zone,name,type,data%s,aux,ttl,active) "
      "VALUES (%u,'%s','%s','%s'%s%s%s,%u,%u,'%s');";
#if DEBUG_SQL
    Debug(querystr, mydns_rr_table_name, (edatalen)?",edata":"",
	  import_zone_id, esc_name, type, esc_data,
	  (edatalen)?",'":"",
	  (edatalen)?esc_edata:"",
	  (edatalen)?"'":"",
	  aux, ttl, ACTIVE);
#endif
    sql_queryf(sql,
	       querystr, mydns_rr_table_name, (edatalen)?",edata":"",
	       import_zone_id, esc_name, type, esc_data,
	       (edatalen)?",'":"",
	       (edatalen)?esc_edata:"",
	       (edatalen)?"'":"",
	       aux, ttl, ACTIVE);
  } else {
    querystr = "INSERT INTO %s (zone,name,type,data%s,aux,ttl) "
      "VALUES (%u,'%s','%s','%s'%s%s%s,%u,%u);";
#if DEBUG_SQL
    Debug(querystr, mydns_rr_table_name, (edatalen)?",edata":"",
	  import_zone_id, esc_name, type, esc_data,
	  (edatalen)?",'":"",
	  (edatalen)?esc_edata:"",
	  (edatalen)?"'":"",
	  aux, ttl);
#endif
    sql_queryf(sql,
	       querystr, mydns_rr_table_name, (edatalen)?",edata":"",
	       import_zone_id, esc_name, type, esc_data,
	       (edatalen)?",'":"",
	       (edatalen)?esc_edata:"",
	       (edatalen)?"'":"",
	       aux, ttl);
  }
  RELEASE(esc_name);
  RELEASE(esc_data);
  RELEASE(esc_edata);
}
/*--- import_rr() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	MAIN
**************************************************************************************************/
int
main(int argc, char **argv) {
  setlocale(LC_ALL, "");				/* Internationalization */
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
  cmdline(argc, argv);
  load_config();

  rr_imported = ptr_imported = 0;

  if (input_format == INPUT_UNKNOWN) {			/* No input format specified */
    Warnx(_("no input format specified"));
    usage(EXIT_FAILURE);
  }

  /* If we're inserting directly into the database, connect to the database */
  if (!opt_output)
    db_connect();

  if (optind >= argc) {					/* No arguments - try to import all zones */
    switch (input_format) {
    case INPUT_AXFR:
      Warnx(_("using AXFR method; no zones specified"));
      usage(EXIT_FAILURE);
      break;

#ifdef TINYDNS_IMPORT
    case INPUT_TINYDNS:
      import_tinydns(tinydns_datafile, NULL);
      break;
#endif

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

#ifdef TINYDNS_IMPORT
    case INPUT_TINYDNS:
      import_tinydns(tinydns_datafile, (char *)argv[optind]);
      break;
#endif

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
