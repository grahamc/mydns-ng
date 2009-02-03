/**************************************************************************************************
	$Id: ptrconvert.c,v 1.14 2005/04/20 17:22:25 bboy Exp $

	Outputs zone data in various formats.

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
#include "libptr.h"

char	*thishostname = NULL;   	                        /* Hostname of local machine */
char	*ns = NULL, *mbox = NULL;				/* Default NS/MBOX */

/* Information about an imported zone */
typedef struct _arpazone
{
	char		name[30];				/* Zone name */
	uint32_t	id;					/* Zone ID */
	uint32_t	low, high;				/* Extent of IP addresses for zone */
} ARPAZONE;

ARPAZONE **Zones = NULL;					/* List of zones to import */
int		numZones = 0;					/* Number of zones in list */

unsigned int zones_found = 0, zones_inserted = 0;		/* Total new zones found and created */
unsigned int rr_found = 0, rr_inserted = 0;			/* Total resource records found/inserted */


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
    printf(_("Usage: %s [OPTIONS]... NS MBOX"), progname);
    puts("");
    puts(_("Convert pre-0.9.12 PTR table format to in-addr.arpa zones."));
    puts("");
    /*	puts("----------------------------------------------------------------------------78");  */
    puts(_("  -c, --conf=FILE         read config from FILE instead of the default"));
    puts(_("  -D, --database=DB       database name to use"));
    puts(_("  -h, --host=HOST         connect to SQL server at HOST"));
    puts(_("  -p, --password=PASS     password for SQL server (or prompt from tty)"));
    puts(_("  -u, --user=USER         username for SQL server if not current user"));
    puts("");
#if DEBUG_ENABLED
    puts(_("  -d, --debug             enable debug output"));
#endif
    puts(_("  -v, --verbose           be more verbose while running (on by default)"));
    puts(_("      --help              display this help and exit"));
    puts(_("      --version           output version information and exit"));
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
    {"conf",			required_argument,	NULL,	'c'},
    {"database",		required_argument,	NULL,	'D'},
    {"host",			required_argument,	NULL,	'h'},
    {"password",		optional_argument,	NULL,	'p'},
    {"user",			required_argument,	NULL,	'u'},
    
    {"debug",			no_argument,		NULL,	'd'},
    {"verbose",			no_argument,		NULL,	'v'},
    {"help",			no_argument,		NULL,	0},
    {"version",			no_argument,		NULL,	0},

    {NULL, 0, NULL, 0}
  };

  /* Set default NS and MBOX based on this machine's hostname */
  thishostname = ALLOCATE(DNS_MAXNAMELEN+1, char[]);
  gethostname(thishostname, DNS_MAXNAMELEN);
  ASPRINTF(&ns, "%s.", thishostname);
  ASPRINTF(&mbox, "hostmaster.%s.", thishostname);

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
      }
      break;

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

    case 'v':								/* -v, --verbose */
      err_verbose = 1;
      break;
    default:
      usage(EXIT_FAILURE);
    }
  }

  /* Get NS and/or MBOX if specified */
  if (optind < argc)
    snprintf(ns, sizeof(ns), "%s", (char *)argv[optind++]);
  if (optind < argc)
    snprintf(mbox, sizeof(mbox), "%s", (char *)argv[optind++]);
  if (LASTCHAR(ns) != '.')
    strncat(ns, ".", sizeof(ns) - strlen(ns) - 1);
  if (LASTCHAR(mbox) != '.')
    strncat(mbox, ".", sizeof(mbox) - strlen(mbox) - 1);
}
/*--- cmdline() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	LOAD_ZONE_LIST
	Examines all records in the 'ptr' table, constructing a list of zones to create.
**************************************************************************************************/
static void
load_zone_list(void) {
  SQL_RES	*res;
  SQL_ROW	row;
  char		*query = NULL;
  size_t	querylen;

  /* Construct and execute query */
  querylen = ASPRINTF(&query,
		      "SELECT "MYDNS_PTR_FIELDS"%s FROM %s",
		      (mydns_ptr_use_active ? ",active" : ""), mydns_ptr_table_name);
  res = sql_query(sql, query, querylen);
  RELEASE(query);
  if (!res)
    ErrSQL(sql, "Error selecting PTR records");

  /* Add results to list */
  while ((row = sql_getrow(res, NULL))) {
    uint32_t		ip;
    uint8_t		quad[4];
    char		*name = NULL;
    register int	n;
    MYDNS_PTR		*ptr;

    if (mydns_ptr_use_active && row[4] && !GETBOOL(row[4]))
      continue;

    if (!(ptr = mydns_parse_ptr(row)))
      continue;

    /* Get IP address etc */
    ip = htonl(ptr->ip);
    memcpy(&quad, &ip, sizeof(quad));

    /* Generate zone name */
    ASPRINTF(&name, "%u.%u.%u.in-addr.arpa.", quad[2], quad[1], quad[0]);

    /* Find zone */
    for (n = 0; n < numZones; n++)
      if (!strcasecmp(Zones[n]->name, name))
	break;

    /* Add new zone if not found */
    if (n == numZones) {
      if (!Zones) {
	Zones = ALLOCATE_N(numZones + 1, sizeof(ARPAZONE *), ARPAZONE*[]);
      } else {
	Zones = REALLOCATE(Zones, (numZones + 1) * sizeof(ARPAZONE *), ARPAZONE*[]);
      }
      Zones[numZones] = ALLOCATE(sizeof(ARPAZONE), ARPAZONE);
      strncpy(Zones[numZones]->name, name, sizeof(Zones[numZones]->name)-1);
      Zones[numZones]->id = 0;

      /* Get low/high extent of zone */
      quad[3] = 0;
      memcpy(&ip, &quad, sizeof(ip));
      Zones[numZones]->low = ntohl(ip);
      quad[3] = 255;
      memcpy(&ip, &quad, sizeof(ip));
      Zones[numZones]->high = ntohl(ip);

      numZones++;
    }
    RELEASE(name);
    RELEASE(ptr);
  }
  sql_free(res);

  if (!numZones)
    Errx("No zones found in PTR table.");

  Verbose("loaded %d distinct zone%s from \"%s\" table",
	  numZones, S(numZones), mydns_ptr_table_name);
}
/*--- load_zone_list() --------------------------------------------------------------------------*/


/**************************************************************************************************
	GET_ZONE_IDS
	For each zone in the 'Zones' list, load the zone ID.
**************************************************************************************************/
static void
get_zone_ids(void) {
  int n;

  for (n = 0; n < numZones; n++) {
    MYDNS_SOA *soa = NULL;

    /* Attempt to load pre-existing SOA */
    if (!mydns_soa_load(sql, &soa, Zones[n]->name) && soa) {
      Verbose("%s: Zone %u", Zones[n]->name, soa->id);
      Zones[n]->id = soa->id;
      mydns_soa_free(soa);

      zones_found++;
    } else {	/* Insert new record */
      long	id;
      uchar	*query;
      int	querylen;
      char	*esc_origin;
      char	*esc_ns, *esc_mbox;
      int	res;

      esc_origin = sql_escstr(sql, Zones[n]->name);
      esc_ns= sql_escstr(sql, ns);
      esc_mbox = sql_escstr(sql, mbox);

      /* Construct and issue query */
      querylen = ASPRINTF((char**)&query,
			  "INSERT INTO %s (origin,ns,mbox) VALUES ('%s','%s','%s')",
			  mydns_soa_table_name, esc_origin, esc_ns, esc_mbox);
      RELEASE(esc_origin);
      RELEASE(esc_ns);
      RELEASE(esc_mbox);
      res = sql_nrquery(sql, (char*)query, querylen);
      RELEASE(query);
      if (res != 0)
	ErrSQL(sql, "%s: Error inserting zone", Zones[n]->name);

      if ((id = sql_count(sql, "SELECT id FROM soa WHERE origin='%s'", esc_origin)) < 0)
	ErrSQL(sql, "%s: Error getting zone ID", Zones[n]->name);
      Zones[n]->id = (uint32_t)id;

      Verbose("%s: Zone %u (new)", Zones[n]->name, Zones[n]->id);
      zones_inserted++;
    }
  }
}
/*--- get_zone_ids() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	PTRCONVERT
	For each zone in 'Zones' list, gets all relevant records from the PTR table and inserts them
	into the appropriate zones.
**************************************************************************************************/
static void
ptrconvert(void) {
  int n;

  for (n = 0; n < numZones; n++) {
    SQL_RES	*res;
    SQL_ROW	row;
    char		*query;
    char		*esc_origin;
    size_t		querylen;
    int		inserted = 0, found = 0;

    esc_origin = sql_escstr(sql, Zones[n]->name);

    querylen = ASPRINTF(&query,
			"SELECT "MYDNS_PTR_FIELDS"%s FROM %s WHERE ip >= %u AND ip <= %u",
			(mydns_ptr_use_active ? ",active" : ""),
			mydns_ptr_table_name, Zones[n]->low, Zones[n]->high);
    RELEASE(esc_origin);
    res = sql_query(sql, query, querylen);
    RELEASE(query);
    if (!(res))
      ErrSQL(sql, "Error selecting PTR records");

    /* Insert RR for each result */
    while ((row = sql_getrow(res, NULL))) {
      MYDNS_PTR	*ptr;
      uint32_t	ip;
      uint8_t		quad[4];
      long		rv;

      if (mydns_ptr_use_active && row[4] && !GETBOOL(row[4]))
	continue;

      /* Get PTR data and quad values */
      if (!(ptr = mydns_parse_ptr(row)))
	continue;
      ip = htonl(ptr->ip);
      memcpy(&quad, &ip, sizeof(quad));

      /* See if this record already exists */
      rv = sql_count(sql,
		     "SELECT COUNT(*) FROM %s"
		     " WHERE zone=%u AND type='PTR' AND (name='%u' OR name='%u.%s')",
		     mydns_rr_table_name, Zones[n]->id, quad[3], quad[3], Zones[n]->name);

      if (rv == 0) {
	char	*esc_data;
	int	sqlres;

	esc_data = sql_escstr(sql, ptr->name);

	querylen = ASPRINTF(&query,
			    "INSERT INTO %s (zone,name,type,data,ttl) VALUES (%u,'%u','PTR','%s',%u)",
			    mydns_rr_table_name, Zones[n]->id, quad[3],
			    esc_data, ptr->ttl);
	RELEASE(esc_data);
	sqlres = sql_nrquery(sql, query, querylen);
	RELEASE(query);
	if (sqlres != 0)
	  ErrSQL(sql, "%s: Error inserting rr", Zones[n]->name);

	inserted++;
	rr_inserted++;
      } else if (rv > 0) {
	found++;
	rr_found++;
      }	else if (rv < 0)
	ErrSQL(sql, "Error looking for existing resource record");
      RELEASE(ptr);
    }

    Verbose("%s: %d record%s (%d found, %d inserted)", Zones[n]->name,
	    found + inserted, S(found + inserted), found, inserted);

    sql_free(res);
  }
}
/*--- ptrconvert() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	MAIN
**************************************************************************************************/
int
main(int argc, char **argv) {
  setlocale(LC_ALL, "");						/* Internationalization */
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
  cmdline(argc, argv);
  load_config();
  mydns_set_ptr_table_name(conf_get(&Conf, "ptr-table", NULL));
  db_connect();

  if (!sql_istable(sql, mydns_ptr_table_name)) {
    Notice(_("No table `%s' in database"), mydns_ptr_table_name);
    Notice(_("Your setup is correct; you don't need to run this program"));
    exit(EXIT_FAILURE);
  }

  if (mydns_set_ptr_use_active(sql) > 0)
    Verbose(_("optional \"active\" column found in \"%s\" table"), mydns_ptr_table_name);

  Verbose("Default nameserver is \"%s\"", ns);
  Verbose("Default mailbox is \"%s\"", mbox);

  load_zone_list();							/* Get list of zones */
  get_zone_ids();							/* Get zone ID for each zone */
  ptrconvert();								/* Insert ptr records */

  Notice("processed %d zone%s (%d found, %d created)",
	 zones_found + zones_inserted, S(zones_found + zones_inserted),
	 zones_found, zones_inserted);

  Notice("processed %d resource record%s (%d found, %d created)",
	 rr_found + rr_inserted, S(rr_found + rr_inserted),
	 rr_found, rr_inserted);
 
  return (0);
}
/*--- main() ------------------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
