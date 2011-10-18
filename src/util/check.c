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

#include "util.h"

MYDNS_SOA	*soa;					/* Current SOA record being scanned */
MYDNS_RR	*rr;					/* Current RR record */
char		*name = NULL;				/* Current expanded name */
char		*data = NULL;				/* Current expanded data */
int		opt_consistency = 0;			/* Consistency check? */
int		opt_consistency_only = 0;		/* Consistency check only? */

#ifdef EXTENDED_CHECK_WRITTEN
int		opt_extended_check = 0;			/* Extended check? */
#endif

int		syntax_errors, consistency_errors;	/* Number of errors found */

static char *
__EXPAND_DATA(char *s) {
  int slen = strlen(s);

  if (*s) slen += 1;
  slen += strlen(soa->origin);

  s = REALLOCATE(s, slen + 1, char[]);
  if (*s) strcat(s, ".");
  strcat(s, soa->origin);

  return s;
}

#define EXPAND_DATA(str) \
			if (!(str)[0] || LASTCHAR((str)) != '.') \
			{ \
			  str=__EXPAND_DATA(str); \
			}


/**********************************************************************************************
	USAGE
	Display program usage information.
**********************************************************************************************/
static void
usage(int status) {
  if (status != EXIT_SUCCESS) {
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
#ifdef EXTENDED_CHECK_WRITTEN
    puts(_("  -x, --extended          extended check for data/name references"));
#endif
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
#ifdef EXTENDED_CHECK_WRITTEN
    {"extended",		no_argument,		NULL,	'x'},
#endif

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
	  usage(EXIT_SUCCESS);
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
#ifdef EXTENDED_CHECK_WRITTEN
    case 'x':							/* -x, --extended */
      opt_extended_check = 1;
      break;
#endif
    default:
      usage(EXIT_FAILURE);
    }
  }
  load_config();
}
/*--- cmdline() -----------------------------------------------------------------------------*/


/**********************************************************************************************
	RRPROBLEM
	Output a string describing a problem found.
**********************************************************************************************/
static void rrproblem(const char *fmt, ...) __printflike(1,2);
static void
rrproblem(const char *fmt, ...) {
  va_list ap;

  meter(0,0);

  va_start(ap, fmt);
  vprintf(fmt, ap);						/* 1. message */
  va_end(ap);
  printf("\t");

  if (soa)							/* 2. soa id */
    printf("%u\t", soa->id);
  else
    printf("-\t");

  if (rr)							/* 3. rr id */
    printf("%u\t", rr->id);
  else
    printf("-\t");

  printf("%s\t", *name ? name : "-");				/* 4. name */

  if (soa || rr)						/* 5. ttl */
    printf("%u\t", rr ? rr->ttl : soa->ttl);
  else
    printf("-\t");

  printf("%s\t", rr ? mydns_qtype_str(rr->type) : "-");		/* 6. rr type */
  printf("%s\n", (data && *data) ? data : "-");			/* 7. data */

  fflush(stdout);

  syntax_errors++;
}
/*--- rrproblem() ---------------------------------------------------------------------------*/


#ifdef EXTENDED_CHECK_WRITTEN
/**********************************************************************************************
	CHECK_NAME_EXTENDED
**********************************************************************************************/
static void
check_name_extended(const char *name_in, const char *fqdn, const char *col) {
  /* XXX: Add check to detect names that we should be authoritative for but
     that do not have records */
}
/*--- check_name_extended() -----------------------------------------------------------------*/
#endif


/**********************************************************************************************
	SHORTNAME
	Removes the origin from a name if it is present.
	If empty_name_is_ok is nonzero, then return "" if the empty string matches the origin.
**********************************************************************************************/
static char *
shortname(char *name_to_shorten, int empty_name_is_ok) {
  size_t nlen = strlen(name_to_shorten), olen = strlen(soa->origin);

  if (nlen < olen) 
    return (name_to_shorten);
  if (!strcasecmp(soa->origin, name_to_shorten)) {
    if (empty_name_is_ok) 
      return ((char*)"");
    else 
      return (name_to_shorten);
  }
  if (!strcasecmp(name_to_shorten + nlen - olen, soa->origin))
    name_to_shorten[nlen - olen - 1] = '\0';
  return (name_to_shorten);
}
/*--- shortname() ---------------------------------------------------------------------------*/


/**********************************************************************************************
	CHECK_NAME
	Verifies that "name" is a valid name.
**********************************************************************************************/
static void
check_name(const char *name_in, const char *col, int is_rr, int allow_underscore) {
  char		*buf, *b, *label;
  char		*fqdn;
  int		fqdnlen;
  
  fqdnlen = strlen(name_in);
  if (is_rr && *name_in && LASTCHAR(name_in) != '.') fqdnlen += strlen(soa->origin) + 1;

  fqdn = ALLOCATE(fqdnlen + 1, char[]);
  memset(fqdn, 0, fqdnlen + 1);

  strncpy(fqdn, name_in, fqdnlen);

  /* If last character isn't '.', append the origin */
  if (is_rr && *fqdn && LASTCHAR(fqdn) != '.') {
    strcat(fqdn, ".");
    strcat(fqdn, soa->origin);
  }

  if (!strlen(fqdn))
    return rrproblem(_("FQDN in `%s' is empty"), col);

  if (strlen(fqdn) > DNS_MAXNAMELEN)
    return rrproblem(_("FQDN in `%s' is too long"), col);

  /* Break into labels, verifying each */
  if (strcmp(fqdn, ".")) {
    buf = STRDUP(fqdn);
    for (b = buf; (label = strsep(&b, ".")); ) {
      register int len = strlen(label);
      register char *cp;

      if (!b) {		/* Last label - should be the empty string */
	if (strlen(label))
	  rrproblem(_("Last label in `%s' not the root zone"), col);
	break;
      }
      if (strcmp(label, "*")) {
	if (len > DNS_MAXLABELLEN)
	  rrproblem(_("Label in `%s' is too long"), col);
	if (len < 1)
	  rrproblem(_("Blank label in `%s'"), col);
	for (cp = label; *cp; cp++) {
	  if (*cp == '-' && cp == label)
	    rrproblem(_("Label in `%s' begins with a hyphen"), col);
	  if (*cp == '-' && ((cp - label) == len-1))
	    rrproblem(_("Label in `%s' ends with a hyphen"), col);
	  if (!isalnum((int)(*cp)) && *cp != '-') {
	    if (is_rr && *cp == '*')
	      rrproblem(_("Wildcard character `%c' in `%s' not alone"), *cp, col);
	    else if (*cp == '_' && allow_underscore)
	      ;
	    else
	      rrproblem(_("Label in `%s' contains illegal character `%c'"), col, *cp);
	  }
	}
      } else if (!is_rr)
	rrproblem(_("Wildcard not allowed in `%s'"), col);
    }
    RELEASE(buf);
  }

#ifdef EXTENDED_CHECK_WRITTEN
  /* If extended check, do extended check */
  if (is_rr && opt_extended_check)
    check_name_extended(name_in, fqdn, col);
#endif
  RELEASE(fqdn);
}
/*--- check_name() ------------------------------------------------------------------------------*/


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
  check_name(soa->ns, "soa.ns", 0, 0);
  check_name(soa->mbox, "soa.mbox", 0, 0);

  if (LASTCHAR(name) != '.')
    rrproblem(_("soa.origin is not a FQDN (no trailing dot)"));

  if (soa->refresh < 300) rrproblem(_("soa.refresh is less than 300 seconds"));
  if (soa->retry < 300) rrproblem(_("soa.retry is less than 300 seconds"));
  if (soa->expire < 300) rrproblem(_("soa.expire is less than 300 seconds"));
  if (soa->minimum < 300) rrproblem(_("soa.minimum is less than 300 seconds"));
  if (soa->ttl < 300) rrproblem(_("soa.ttl is less than 300 seconds"));
  if (soa->minimum < 300) rrproblem(_("soa.minimum is less than 300 seconds"));

  return (soa);
}
/*--- check_soa() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_RR_CNAME
	Expanded check for CNAME resource record.
**************************************************************************************************/
static void
check_rr_cname(void) {
  char *xname;
  int found = 0;

  EXPAND_DATA(data);
  check_name(data, "rr.data", 1, 0);

  /* A CNAME record can't have any other type of RR data for the same name */
  xname = sql_escstr(sql, (char *)name);
  found = sql_count(sql,
		    "SELECT COUNT(*) FROM %s WHERE zone=%u AND name='%s' AND type != 'CNAME' AND type != 'ALIAS'",
		    mydns_rr_table_name, rr->zone, xname);
  RELEASE(xname);
  /* If not found that way, check short name */
  if (!found) {
    xname = sql_escstr(sql, (char *)shortname(name, 1));
    found = sql_count(sql,
		      "SELECT COUNT(*) FROM %s WHERE zone=%u AND name='%s' AND type != 'CNAME' AND type != 'ALIAS'",
		      mydns_rr_table_name, rr->zone, xname);
    RELEASE(xname);
    EXPAND_DATA(name);
  }

  if (found)
    rrproblem(_("non-CNAME record(s) present alongside CNAME"));
}
/*--- check_rr_cname() --------------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_RR_HINFO
	Expanded check for HINFO resource record.
**************************************************************************************************/
static void
check_rr_hinfo(void) {
  char	os[DNS_MAXNAMELEN + 1] = "", cpu[DNS_MAXNAMELEN + 1] = "";

  if (hinfo_parse(MYDNS_RR_DATA_VALUE(rr), cpu, os, DNS_MAXNAMELEN) < 0)
    rrproblem(_("data too long in HINFO record"));
}
/*--- check_rr_hinfo() --------------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_RR_NAPTR
	Expanded check for NAPTR resource record.
**************************************************************************************************/
static void
check_rr_naptr(void) {
  char *tmp, *data_copy, *p;

  data_copy = STRNDUP(MYDNS_RR_DATA(rr), MYDNS_RR_DATA_LENGTH(rr));
  p = data_copy;

  if (!strsep_quotes2(&p, &tmp))
    return rrproblem(_("'order' field missing from NAPTR record"));
  RELEASE(tmp);

  if (!strsep_quotes2(&p, &tmp))
    return rrproblem(_("'preference' field missing from NAPTR record"));
  RELEASE(tmp);

  if (!strsep_quotes2(&p, &tmp))
    return rrproblem(_("'flags' field missing from NAPTR record"));
  RELEASE(tmp);

  if (!strsep_quotes2(&p, &tmp))
    return rrproblem(_("'service' field missing from NAPTR record"));
  RELEASE(tmp);

  if (!strsep_quotes2(&p, &tmp))
    return rrproblem(_("'regexp' field missing from NAPTR record"));
  RELEASE(tmp);

  if (!strsep_quotes2(&p, &tmp))
    return rrproblem(_("'replacement' field missing from NAPTR record"));
  RELEASE(tmp);

  /* For now, don't check 'replacement'.. the example in the RFC even contains illegal chars */
  /* EXPAND_DATA(tmp); */
  /* check_name(tmp, "replacement", 1, 0); */
  RELEASE(data_copy);
}
/*--- check_rr_naptr() --------------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_RR
	Check an individual resource record.
**************************************************************************************************/
static void
check_rr(void) {
  /* Expand RR's name into `name' */
  int	namelen = strlen(MYDNS_RR_NAME(rr));

  name = REALLOCATE(name, namelen+1, char[]);
  memset(name, 0, namelen+1);
  data = REALLOCATE(data, MYDNS_RR_DATA_LENGTH(rr)+1, char[]);
  memset(data, 0, MYDNS_RR_DATA_LENGTH(rr)+1);
  strncpy(name, MYDNS_RR_NAME(rr), namelen);
  strncpy(data, MYDNS_RR_DATA_VALUE(rr), MYDNS_RR_DATA_LENGTH(rr));
  EXPAND_DATA(name);
  switch (rr->type) {
  case DNS_QTYPE_TXT:
  case DNS_QTYPE_SRV:
    check_name(name, "rr.name", 1, 1);
    break;
  default:
    check_name(name, "rr.name", 1, 0);
  }

  if (!ignore_minimum && (rr->ttl < soa->minimum))
    rrproblem(_("TTL below zone minimum"));

  switch (rr->type) {
  case DNS_QTYPE_A:							/* Data: IPv4 address */
    {
      struct in_addr addr;
#if ALIAS_ENABLED
      if (rr->alias == 1)
	check_rr_cname();
      else {
#endif /* ALIAS_ENABLED */
	if (inet_pton(AF_INET, data, (void *)&addr) <= 0)
	  rrproblem(_("IPv4 address in `data' is invalid"));
#if ALIAS_ENABLED
      }
#endif /* ALIAS_ENABLED */
    }
    break;

  case DNS_QTYPE_AAAA:							/* Data: IPv6 address */
    {
      uint8_t addr[16];
      if (inet_pton(AF_INET6, data, (void *)&addr) <= 0)
	rrproblem(_("IPv6 address in `data' is invalid"));
    }
    break;

  case DNS_QTYPE_CNAME:							/* Data: Name */
    check_rr_cname();
    break;

  case DNS_QTYPE_HINFO:							/* Data: Host info */
    check_rr_hinfo();
    break;

  case DNS_QTYPE_MX:							/* Data: Name */
    EXPAND_DATA(data);
    check_name(data, "rr.data", 1, 0);
    break;

  case DNS_QTYPE_NAPTR:							/* Data: Multiple fields */
    check_rr_naptr();
    break;

  case DNS_QTYPE_NS:							/* Data: Name */
    EXPAND_DATA(data);
    check_name(data, "rr.data", 1, 0);
    break;

  case DNS_QTYPE_PTR:							/* Data: PTR */
    /* TODO */
    break;

  case DNS_QTYPE_RP:							/* Data: Responsible person */
    {
      char	*txt;

      txt = ALLOCATE(strlen(MYDNS_RR_RP_TXT(rr)) + 1, char[]);
      strcpy(txt, MYDNS_RR_RP_TXT(rr));
      EXPAND_DATA(txt);
      check_name(data, "rr.data (mbox)", 1,0 );
      check_name(txt, "rr.data (txt)", 1, 0);
      RELEASE(txt);
    }
    break;

  case DNS_QTYPE_SRV:							/* Data: Server location */
    /* TODO */
    break;

  case DNS_QTYPE_TXT:							/* Data: Undefined text string */
    /*
     * Data length must be less than DNS_MAXTXTLEN
     * and each element must be less than DNS_MAXTXTELEMLEN
     */
    if (MYDNS_RR_DATA_LENGTH(rr) > DNS_MAXTXTLEN)
      rrproblem(_("Text record length is too great"));
    {
      char	*txt = MYDNS_RR_DATA_VALUE(rr);
      uint16_t	txtlen = MYDNS_RR_DATA_LENGTH(rr);

      while (txtlen > 0) {
	uint16_t len = strlen(txt);
	if (len > DNS_MAXTXTELEMLEN)
	  rrproblem(_("Text element in TXT record is too long"));
	txt = &txt[len];
	txtlen -= len;
      }
    }
    break;

  default:
    rrproblem(_("Unknown/unsupported resource record type"));
    break;
  }
}
/*--- check_rr() --------------------------------------------------------------------------------*/

                                                                                                                               

/**************************************************************************************************
	CHECK_ZONE
	Checks each RR in the current zone through check_rr.
**************************************************************************************************/
static void
check_zone(void) {
  char *query;
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
