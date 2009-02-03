/**************************************************************************************************
	$Id: import-tinydns.c,v 1.14 2005/04/20 16:49:12 bboy Exp $

	import-tinydns.c: Import DNS data from a tinydns-data format data file.

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

#ifdef TINYDNS_IMPORT

#define MAX_FIELDS		20					/* Max fields per line */
#define TINYDNS_DEF_REFRESH	16384
#define TINYDNS_DEF_RETRY	2048
#define TINYDNS_DEF_EXPIRE	1048576
#define TINYDNS_DEF_MINIMUM	2560
#define TINYDNS_DEF_TTL		2560

typedef struct _zone
{
	uint32_t id;
	char		*origin, *ns, *mbox;
	uint32_t	serial, refresh, retry, expire, minimum, ttl;
} ZONE;

static ZONE		**Zones = NULL;					/* List of zones found */
static int		numZones = 0;					/* Number of zones found */

static uint32_t		default_serial;					/* Default serial */
static unsigned int	lineno;						/* Current line number */
static char		*filename;					/* Input file name */
static uchar		*field[MAX_FIELDS];				/* Fields parsed from line */

static char		tinydns_zone[DNS_MAXNAMELEN+1];			/* Zone to import (only this zone) */

extern uint32_t		import_zone_id;					/* ID of current zone */



/**************************************************************************************************
	TWARN
	Convenience function; outputs error message for current file/line.
**************************************************************************************************/
static void
twarn(const char *fmt, ...) {
  va_list	ap;
  char		buf[1024];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  va_end(ap);

  Warnx("%s: line %u: %s", filename, lineno, buf);
  return;
}
/*--- twarn() -----------------------------------------------------------------------------------*/


/**************************************************************************************************
	ZONE_OK
	Given an fqdn, is this a zone we want to process?
**************************************************************************************************/
static inline int
zone_ok(const uchar *fqdn) {
  if (!tinydns_zone[0])							/* Processing all zones */
    return 1;
  if (!strcasecmp((char*)fqdn, tinydns_zone))					/* Exact match on zone */
    return 1;

  /* Does FQDN at least end with the zone? */
  if (strlen((char*)fqdn) > strlen(tinydns_zone)
      && !strcasecmp((char*)(fqdn + strlen((char*)fqdn) - strlen(tinydns_zone)), tinydns_zone))
    return 1;

  return 0;
}
/*--- zone_ok() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	ZONECMP
	Comparison function for sorting zones.
**************************************************************************************************/
static int
zonecmp(const void *p1, const void *p2) {
  ZONE *z1 = *(ZONE **)p1, *z2 = *(ZONE **)p2;
  return strcasecmp(z1->origin, z2->origin);
}
/*--- zonecmp() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	FIND_ZONE
	Looks for the specified zone.  Returns it if found, else NULL.
**************************************************************************************************/
static ZONE *
find_zone(const uchar *origin) {
  register int n;

  for (n = 0; n < numZones; n++)
    if (!strcasecmp(Zones[n]->origin, (char*)origin))
      return Zones[n];
  return NULL;
}
/*--- find_zone() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	FIND_HOST_ZONE
	Looks for the zone matching the provided FQDN.  Returns it if found, else NULL.  Also sets
	the hostname part of 'fqdn' in 'hostname'.
**************************************************************************************************/
static ZONE *
find_host_zone(uchar *fqdn, uchar **local_hostname) {
  uchar *c;
  ZONE *z;

  *local_hostname = (uchar*)STRDUP("");

  if ((z = find_zone(fqdn)))				/* See if 'fqdn' is a plain zone origin */
    return (z);

  for (c = fqdn; *c; c++)
    if (*c == '.')
      if ((z = find_zone(c + 1))) {
	REALLOCATE(*local_hostname, c - fqdn + 1, char[]);
	memcpy(*local_hostname, fqdn, c - fqdn);
	(*local_hostname)[c - fqdn] = '\0';
	return (z);
      }

  RELEASE(*local_hostname);
  return NULL;
}
/*--- find_host_zone() --------------------------------------------------------------------------*/


/**************************************************************************************************
	FIND_ARPA_ZONE
	Given an IP address in numbers-and-dots format, returns the relevant in-addr.arpa zone, or
	NULL if not found.
**************************************************************************************************/
static inline ZONE *
find_arpa_zone(const uchar *ip, uchar **local_hostname) {
  uchar *zone = NULL, *buf = NULL, *p, *a, *b, *c, *d;
  ZONE *z;

  *local_hostname = NULL;

  buf = (uchar*)STRDUP((char*)ip);

  p = buf;
  if (!(a = (uchar*)strsep((char**)&p, ".")) || !(b = (uchar*)strsep((char**)&p, "."))
      || !(c = (uchar*)strsep((char**)&p, ".")) || !(d = (uchar*)strsep((char**)&p, "."))) {
    RELEASE(buf);
    return NULL;
  }

  ASPRINTF((char**)&zone, "%s.in-addr.arpa", a);
  z = find_zone(zone);
  RELEASE(zone);
  if (z) {
    ASPRINTF((char**)local_hostname, "%s.%s.%s", d, c, b);
    return z;
  }

  ASPRINTF((char**)&zone, "%s.%s.in-addr.arpa", b, a);
  z = find_zone(zone);
  RELEASE(zone);
  if (z) {
    ASPRINTF((char**)local_hostname, "%s.%s", d, c);
    return z;
  }

  ASPRINTF((char**)&zone, "%s.%s.%s.in-addr.arpa", c, b, a);
  z = find_zone(zone);
  RELEASE(zone);
  if (z) {
    *local_hostname = (uchar*)STRDUP((char*)d);
    return z;
  }

  return NULL;
}
/*--- find_arpa_zone() --------------------------------------------------------------------------*/


/**************************************************************************************************
	NEW_ZONE
	Adds a new zone.  Returns the zone.
**************************************************************************************************/
static ZONE *
new_zone(const uchar *originp) {
  ZONE *z;
  uchar *origin = NULL;

  /* If this is an 'in-addr.arpa' zone, knock off the first quad if present */
  if (strlen((char*)originp) > 12 && !strcasecmp((char*)(originp + strlen((char*)originp) - 13), ".in-addr.arpa")) {
    uchar *buf = NULL, *p, *a, *b, *c, *d;

    buf = (uchar*)STRDUP((char*)originp);
    p = buf;
    if ((d = (uchar*)strsep((char**)&p, ".")) && (c = (uchar*)strsep((char**)&p, "."))
	&& (b = (uchar*)strsep((char**)&p, ".")) && (a = (uchar*)strsep((char**)&p, ".")))
      ASPRINTF((char**)&origin, "%s.%s.%s.in-addr.arpa", c, b, a);
    else
      origin = (uchar*)STRDUP((char*)originp);
    RELEASE(buf);
  } else
    origin = (uchar*)STRDUP((char*)originp);

  if ((z = find_zone(origin))) {
    RELEASE(origin);
    return z;
  }

  z = ALLOCATE(sizeof(ZONE), ZONE);

  z->id = 0;
  z->origin = (char*)origin;
  z->ns = NULL;
  z->mbox = NULL;
  z->serial = default_serial;
  z->refresh = TINYDNS_DEF_REFRESH;
  z->retry = TINYDNS_DEF_RETRY;
  z->expire = TINYDNS_DEF_EXPIRE;
  z->minimum = TINYDNS_DEF_MINIMUM;
  z->ttl = TINYDNS_DEF_TTL;

  if (!Zones) {
    Zones = ALLOCATE((numZones + 1) * sizeof(ZONE), ZONE[]);
  } else {
    Zones = REALLOCATE(Zones, (numZones + 1) * sizeof(ZONE), ZONE{});
  }
  Zones[numZones++] = z;
  return z;
}
/*--- new_zone() --------------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_ADD_SOA
	Adds a zone to 'Zones' due to a '.' or 'Z' line.
**************************************************************************************************/
static void
tinydns_add_soa(char *line) {
  char	*l = line + 1;
  ZONE	*z;

  if (line[0] == '.') {							/* '.' (NS) line */
    uchar *fqdn = NULL, *ip = NULL, *x = NULL, *ttl = NULL, *timestamp = NULL, *lo = NULL;

    if (!(fqdn = (uchar*)strsep(&l, ":")) || !strlen((char*)fqdn))
      return twarn("'.' line has no fqdn");

    if (!zone_ok(fqdn))
      return;

    if (!(ip = (uchar*)strsep(&l, ":")))
      return twarn("'.' line has no ip");

    if ((x = (uchar*)strsep(&l, ":")) && (ttl = (uchar*)strsep(&l, ":")) && (timestamp = (uchar*)strsep(&l, ":"))
	&& (lo = (uchar*)strsep(&l, ":"))) {
      /* DONOTHING */;
    }

    z = new_zone(fqdn);

    /* Assign 'ns' */
    if (!z->ns) {
      if (!x || !strlen((char*)x))
	sdprintf(&z->ns, "ns.%s", fqdn);
      else if (strchr((char*)x, '.')) {
	z->ns = STRDUP((char*)x);
      } else
	sdprintf(&z->ns, "%s.ns.%s", x, fqdn);
    }

    /* Assign 'mbox' */
    if (!z->mbox)
      sdprintf(&z->mbox, "hostmaster.%s", fqdn);
  } else if (line[0] == 'Z') {						/* 'Z' (SOA) line */
    uchar *fqdn = NULL, *ns = NULL, *mbox = NULL, *serial = NULL, *refresh = NULL,
      *retry = NULL, *expire = NULL, *minimum = NULL, *ttl = NULL, *timestamp = NULL,
      *lo = NULL;

    if (!(fqdn = (uchar*)strsep((char**)&l, ":")) || !strlen((char*)fqdn))
      return twarn("'Z' line has no fqdn");

    if (!zone_ok(fqdn))
      return;

    if (!(ns = (uchar*)strsep((char**)&l, ":")) || !strlen((char*)ns))
      return twarn("'Z' line has no ns");
    if (!(mbox = (uchar*)strsep((char**)&l, ":")) || !strlen((char*)mbox))
      return twarn("'Z' line has no mbox");

    if ((serial = (uchar*)strsep((char**)&l, ":")) && (refresh = (uchar*)strsep((char**)&l, ":")) && (retry = (uchar*)strsep((char**)&l, ":"))
	&& (expire = (uchar*)strsep((char**)&l, ":")) && (minimum = (uchar*)strsep((char**)&l, ":")) && (ttl = (uchar*)strsep((char**)&l, ":"))
	&& (timestamp = (uchar*)strsep((char**)&l, ":")) && (lo = (uchar*)strsep((char**)&l, ":"))) {
      /* DONOTHING */;
    }

    z = new_zone(fqdn);
    if (!z->ns) {
      z->ns = STRDUP((char*)ns);
    }
    if (!z->mbox) {
      z->mbox = STRDUP((char*)mbox);
    }
    if (serial) z->serial = atol((char*)serial);
    if (refresh) z->refresh = atol((char*)refresh);
    if (retry) z->retry = atol((char*)retry);
    if (expire) z->expire = atol((char*)expire);
    if (minimum) z->minimum = atol((char*)minimum);
    if (ttl) z->ttl = atol((char*)ttl);
  }
}
/*--- tinydns_add_soa() -------------------------------------------------------------------------*/


/**************************************************************************************************
	CREATE_ZONE
	Loads or creates the actual SOA record and loads the ID.
**************************************************************************************************/
static void
create_zone(ZONE *z) {
  uchar *origin = NULL, *ns = NULL, *mbox = NULL;

  if (!z->ns)
    sdprintf(&z->ns, "ns.%s", z->origin);
  if (!z->mbox)
    sdprintf(&z->mbox, "hostmaster.%s", z->origin);

  ASPRINTF((char**)&origin, "%s.", z->origin);
  ASPRINTF((char**)&ns, "%s.", z->ns);
  ASPRINTF((char**)&mbox, "%s.", z->mbox);

  z->id = import_soa(origin, ns, mbox,
		     z->serial, z->refresh, z->retry, z->expire, z->minimum, z->ttl);

  RELEASE(origin);
  RELEASE(ns);
  RELEASE(mbox);
}
/*--- create_zone() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_PLUS

	+fqdn:ip:ttl:timestamp:lo

	Alias fqdn with IP address ip. This is just like =fqdn:ip:ttl except that tinydns-data
	does not create the PTR record.
**************************************************************************************************/
static void
tinydns_plus(void) {
  ZONE	*z;
  uchar	*fqdn = field[0], *ip = field[1], *ttl = field[2];
  uchar	*local_hostname = NULL;

  if (!fqdn || !strlen((char*)fqdn))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
  if (!zone_ok(fqdn))
    return;
  if (!(z = find_host_zone(fqdn, &local_hostname)))
    return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));

  /* Set import_zone_id and import the record */
  if (ip && strlen((char*)ip)) {
    import_zone_id = z->id;
    import_rr(local_hostname, (uchar*)"A", ip, strlen((char*)ip), 0, ttl ? atol((char*)ttl) : (long)z->ttl);
  }
  RELEASE(local_hostname);
}
/*--- tinydns_plus() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_DOT

	.fqdn:ip:x:ttl:timestamp:lo

	1. Create an NS record showing x.ns.fqdn as a name server for fqdn
	2. Create an A record showing ip as the IP address of x.ns.fqdn
**************************************************************************************************/
static void
tinydns_dot(void) {
  ZONE	*z;
  uchar	*fqdn = field[0], *ip = field[1], *x = field[2], *ttl = field[3];
  uchar	*local_hostname = NULL, *buf = NULL;

  if (!fqdn || !strlen((char*)fqdn))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
  if (!zone_ok(fqdn))
    return;

  if (!(z = find_host_zone(fqdn, &local_hostname)))
    return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
  import_zone_id = z->id;

  /* Create NS record */
  if (x && strlen((char*)x)) {
    if (strchr((char*)x, '.')) {
      if (LASTCHAR((char*)x) != '.') {
	ASPRINTF((char**)&buf, "%s.", x);
      } else {
	buf = (uchar*)STRDUP((char*)x);
      }
    } else {
      ASPRINTF((char**)&buf, "%s.ns.", x);
    }
  } else {
    buf = (uchar*)STRDUP("ns");
  }
  import_rr(local_hostname, (uchar*)"NS", buf, strlen((char*)buf), 0, ttl ? atol((char*)ttl) : (long)z->ttl);

  RELEASE(local_hostname);

  /* Create A record if 'ip' is present */
  if (ip && strlen((char*)ip))
    import_rr(buf, (uchar*)"A", ip, strlen((char*)ip), 0, ttl ? atol((char*)ttl) : (long)z->ttl);

  RELEASE(buf);
}
/*--- tinydns_dot() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_AMP

	&fqdn:ip:x:ttl:timestamp:lo

	Name server for domain fqdn. tinydns-data creates

	1. An NS record showing x.ns.fqdn as a name server for fqdn.
	2. An A record showing ip as the IP address of x.ns.fqdn.
**************************************************************************************************/
static void
tinydns_amp(void) {
  ZONE	*z;
  uchar	*fqdn = field[0], *ip = field[1], *x = field[2], *ttl = field[3];
  uchar	*local_hostname = NULL, *buf = NULL;

  if (!fqdn || !strlen((char*)fqdn))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
  if (!zone_ok(fqdn))
    return;

  if (!(z = find_host_zone(fqdn, &local_hostname)))
    return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
  import_zone_id = z->id;

  /* Create NS record */
  if (x && strlen((char*)x)) {
    if (strchr((char*)x, '.')) {
      if (LASTCHAR((char*)x) != '.') {
	ASPRINTF((char**)&buf, "%s.", x);
      } else {
	buf = (uchar*)STRDUP((char*)x);
      }
    } else {
      ASPRINTF((char**)&buf, "%s.ns", x);
    }
  } else {
    buf = (uchar*)STRDUP("ns");
  }

  import_rr(local_hostname, (uchar*)"NS", buf, strlen((char*)buf), 0, ttl ? atol((char*)ttl) : (long)z->ttl);

  RELEASE(local_hostname);

  /* Create A record if 'ip' is present */
  if (ip && strlen((char*)ip))
    import_rr(buf, (uchar*)"A", ip, strlen((char*)ip), 0, ttl ? atol((char*)ttl) : (long)z->ttl);

  RELEASE(buf);
}
/*--- tinydns_amp() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_EQUAL

	=fqdn:ip:ttl:timestamp:lo

	Host 'fqdn' with IP address 'ip'.  Creates:

	1. An A record showing 'ip' as the IP address of 'fqdn'
	2. A PTR record showing 'fqdn' as the name of d.c.b.a.in-addr.arpa if 'ip' is a.b.c.d. 

	Remember to specify name servers for some suffix of 'fqdn'; otherwise tinydns will not respond
	to queries about 'fqdn'.  The same comment applies to other records described below. 
	Similarly, remember to specify name servers for some suffix of d.c.b.a.in-addr.arpa, if that
	domain has been delegated to you.

	Example:

		=button.panic.mil:1.8.7.108

	creates an A record showing 1.8.7.108 as the IP address of button.panic.mil, and a PTR record
	showing button.panic.mil as the name of 108.7.8.1.in-addr.arpa.
**************************************************************************************************/
static void
tinydns_equal(void) {
  ZONE	*z;
  uchar	*fqdn = field[0], *ip = field[1], *ttl = field[2];
  uchar	*local_hostname = NULL;

  if (!fqdn || !strlen((char*)fqdn))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
  if (!zone_ok(fqdn))
    return;

  if (!(z = find_host_zone(fqdn, &local_hostname)))
    return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
  import_zone_id = z->id;

  RELEASE(local_hostname);

  /* Create A record showing 'ip' as the IP address of 'fqdn' */
  import_rr(fqdn, (uchar*)"A", ip, strlen((char*)ip), 0, ttl ? atol((char*)ttl) : (long)z->ttl);

  /* Create PTR record if we can find an appropriate in-addr.arpa zone */
  if ((z = find_arpa_zone(ip, &local_hostname))) {
    import_zone_id = z->id;
    import_rr(local_hostname, (uchar*)"PTR", fqdn, strlen((char*)fqdn), 0, ttl ? atol((char*)ttl) : (long)z->ttl);
  } else {
    uchar *buf = NULL, *p, *a, *b, *c, *d;

    buf = (uchar*)STRDUP((char*)ip);
    p = buf;
    if (!(a = (uchar*)strsep((char**)&p, ".")) || !(b = (uchar*)strsep((char**)&p, "."))
	|| !(c = (uchar*)strsep((char**)&p, ".")) || !(d = (uchar*)strsep((char**)&p, "."))) {
      RELEASE(buf);
      return;
    }
    Warnx("%s:%u: %s.%s.%s.%s.in-addr.arpa: %s", filename, lineno,
	  d, c, b, a, _("no matching zone for PTR record"));
    RELEASE(buf);
  }
  RELEASE(local_hostname);
}
/*--- tinydns_equal() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_AT

	@fqdn:ip:x:dist:ttl:timestamp:lo

	Mail exchanger for 'fqdn'.  Creates:

	1. An MX record showing x.mx.fqdn as a mail exchanger for 'fqdn' at distance 'dist' and
	2. An A record showing 'ip' as the IP address of x.mx.fqdn. 

	You may omit dist; the default distance is 0.

	If 'x' contains a dot then it is treated specially; see above.

	You may create several MX records for 'fqdn', with a different 'x' for each server.
	Make sure to arrange for the SMTP server on each IP address to accept mail for 'fqdn'.

	Example:

		@panic.mil:1.8.7.88:mail.panic.mil

	creates an MX record showing mail.panic.mil as a mail exchanger for panic.mil at distance 0,
	and an A record showing 1.8.7.88 as the IP address of mail.panic.mil.
**************************************************************************************************/
static void
tinydns_at(void) {
  ZONE	*z;
  uchar	*fqdn = field[0], *ip = field[1], *x = field[2], *dist = field[3], *ttl = field[4];
  uchar	*local_hostname = NULL, *mx = NULL;

  if (!fqdn || !strlen((char*)fqdn))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
  if (!zone_ok(fqdn))
    return;

  if (!x || !strlen((char*)x))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("no mail exchanger specified"));

  if (!(z = find_host_zone(fqdn, &local_hostname)))
    return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
  import_zone_id = z->id;

  /* Create MX record showing 'x'.mx.fqdn as MX for 'fqdn' */
  if (strchr((char*)x, '.')) {
    if (LASTCHAR((char*)x) != '.') {
      ASPRINTF((char**)&mx, "%s.", x);
    } else {
      mx = (uchar*)STRDUP((char*)x);
    }
  } else {
    /* Assumes origin terminate by '.' */
    ASPRINTF((char**)&mx, "%s.mx.%s", x, z->origin);
  }

  import_rr(local_hostname, (uchar*)"MX", mx, strlen((char*)mx), dist ? atol((char*)dist) : 0, ttl ? atol((char*)ttl) : (long)z->ttl);

  /* Create A record for 'mx' */
  if (ip && strlen((char*)ip)) {
    RELEASE(local_hostname);
    if (!(z = find_host_zone(mx, &local_hostname))) {
      RELEASE(mx);
      return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
    }
    import_zone_id = z->id;
    import_rr(mx, (uchar*)"A", ip, strlen((char*)ip), 0, ttl ? atol((char*)ttl) : (long)z->ttl);
  }
  RELEASE(local_hostname);
  RELEASE(mx);
}
/*--- tinydns_at() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_APOS

	'fqdn:str:ttl:timestamp:lo

	Creates a TXT record for 'fqdn' containing the string 'str'.  You may use octal \nnn codes to
	include arbitrary bytes inside 'str'; for example, \072 is a colon.
**************************************************************************************************/
static void
tinydns_apos(void) {
  ZONE	*z;
  uchar	*fqdn = field[0], *str = field[1], *ttl = field[2], *txt;
  uchar	*local_hostname = NULL;
  register uchar *s, *d;

  if (!fqdn || !strlen((char*)fqdn))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
  if (!zone_ok(fqdn))
    return;

  if (!str || !strlen((char*)str))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("no text data specified"));

  if (!(z = find_host_zone(fqdn, &local_hostname)))
    return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
  import_zone_id = z->id;

  /* Unescape octal chars */
  txt = ALLOCATE(strlen((char*)str) + 1, char[]);
  for (s = str, d = txt; *s; s++) {
    if (s[0] == '\\' && (s[1] >= '0' && s[1] <= '7')
	&& (s[2] >= '0' && s[2] <= '7') && (s[3] >= '0' && s[3] <= '7')) {
      *d++ = strtol((char*)(s + 1), NULL, 8);
      s += 3;
    } else
      *d++ = *s;
  }
  *d = '\0';
  import_rr(local_hostname, (uchar*)"TXT", txt, strlen((char*)txt), 0, ttl ? atol((char*)ttl) : (long)z->ttl);
  RELEASE(txt);
  RELEASE(local_hostname);
}
/*--- tinydns_apos() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_CARET

	^fqdn:p:ttl:timestamp:lo

	PTR record for fqdn. tinydns-data creates a PTR record for fqdn pointing to the domain name p.
**************************************************************************************************/
static void
tinydns_caret(void) {
  ZONE	*z;
  uchar	*fqdn = field[0], *p = field[1], *ttl = field[2];
  uchar	*local_hostname = NULL, *ptr = NULL;

  if (!fqdn || !strlen((char*)fqdn))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
  if (!zone_ok(fqdn))
    return;

  if (!p || !strlen((char*)p))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("no PTR data specified"));
  ptr = (uchar*)STRDUP((char*)p);
  if (strchr((char*)ptr, '.') && LASTCHAR((char*)ptr) != '.')
    strcat((char*)ptr, ".");

  if (!(z = find_host_zone(fqdn, &local_hostname))) {
    RELEASE(ptr);
    return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
  }
  import_zone_id = z->id;
  import_rr(local_hostname, (uchar*)"PTR", ptr, strlen((char*)ptr), 0, ttl ? atol((char*)ttl) : (long)z->ttl);
  RELEASE(ptr);
  RELEASE(local_hostname);
}
/*--- tinydns_caret() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_C

	Cfqdn:p:ttl:timestamp:lo

	Creates a CNAME record for 'fqdn' pointing to the domain name 'p'.
**************************************************************************************************/
static void
tinydns_C(void) {
  ZONE	*z;
  uchar	*fqdn = field[0], *p = field[1], *ttl = field[2];
  uchar	*local_hostname = NULL, *cname = NULL;

  if (!fqdn || !strlen((char*)fqdn))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
  if (!zone_ok(fqdn))
    return;

  if (!p || !strlen((char*)p))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("no PTR data specified"));
  cname = (uchar*)STRDUP((char*)p);
  if (strchr((char*)cname, '.') && LASTCHAR((char*)cname) != '.')
    strcat((char*)cname, ".");

  if (!(z = find_host_zone(fqdn, &local_hostname))) {
    RELEASE(cname);
    return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
  }
  import_zone_id = z->id;
  import_rr(local_hostname, (uchar*)"CNAME", cname, strlen((char*)cname), 0, ttl ? atol((char*)ttl) : (long)z->ttl);
  RELEASE(cname);
  RELEASE(local_hostname);
}
/*--- tinydns_C() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_COLON

	:fqdn:n:rdata:ttl:timestamp:lo

	Generic record for 'fqdn'.  Creates a record of type 'n' for 'fqdn' showing 'rdata'.
	'n' must be an integer between 1 and 65535; it must not be 2 (NS), 5 (CNAME), 6 (SOA),
	12 (PTR), 15 (MX), or 252 (AXFR).  The proper format of 'rdata' depends on 'n'.  You may use
	octal \nnn codes to include arbitrary bytes inside 'rdata'.
**************************************************************************************************/
static void
tinydns_colon(void) {
  ZONE	*z;
  uchar	*fqdn = field[0]; /* *n = field[1], *rdata = field[2], *ttl = field[3]; */
  uchar	*local_hostname = NULL;

  if (!fqdn || !strlen((char*)fqdn))
    return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
  if (!zone_ok(fqdn))
    return;

  if (!(z = find_host_zone(fqdn, &local_hostname)))
    return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));

  RELEASE(local_hostname);

  Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("generic record type not supported"));
}
/*--- tinydns_colon() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	IMPORT_TINYDNS
	Imports a tinydns-data format file.  Works in two passes: The first pass creates all SOA
	records, the second pass creates all RR records.
**************************************************************************************************/
void
import_tinydns(char *datafile, char *import_zone) {
  struct stat st;
  FILE	*fp;
  char	buf[4096], *b;
  int	n;

  /* Clear zone list */
  for (n = 0; n < numZones; n++) {
    RELEASE(Zones[n]->origin);
    if (Zones[n]->ns)
      RELEASE(Zones[n]->ns);
    if (Zones[n]->mbox)
      RELEASE(Zones[n]->mbox);
    RELEASE(Zones[n]);
  }
  if (Zones)
    RELEASE(Zones);
  Zones = NULL;
  numZones = 0;

  /* Reset globals and open input file */
  lineno = 0;
  filename = datafile;
  if (import_zone)
    strncpy(tinydns_zone, import_zone, sizeof(tinydns_zone)-1);
  else
    tinydns_zone[0] = '\0';
  if (stat(filename, &st))
    return (void)Warn("%s", filename);
  if (!(fp = fopen(filename, "r")))
    return (void)Warn("%s", filename);
  strftime(buf, sizeof(buf)-1, "%Y%m%d", localtime(&st.st_mtime));
  default_serial = atoi(buf);

  /* Pass 1: Search for '.' and 'Z' lines; create SOA records */
  while (fgets(buf, sizeof(buf), fp)) {
    lineno++;
    strtrim(buf);
    if (strlen(buf) < 2)
      continue;
    buf[0] = toupper(buf[0]);
    if (buf[0] == '.' || buf[0] == 'Z')
      tinydns_add_soa(buf);
  }
  if (!numZones) {
    if (import_zone)
      Warnx("%s: %s: %s", filename, import_zone, _("no zone data found"));
    else
      Warnx("%s: %s", filename, _("no zones found"));
    return;
  }
  qsort(Zones, numZones, sizeof(ZONE *), zonecmp);

  /* Load and/or create zones */
  for (n = 0; n < numZones; n++)
    create_zone(Zones[n]);

  if (!import_zone)
    Notice("%s: %u %s", filename, numZones, _("zones"));

  /* Pass 2: Insert resource records */
  rewind(fp);
  lineno = 0;
  while (fgets(buf, sizeof(buf), fp)) {
    lineno++;
    strtrim(buf);
    if (strlen(buf) < 2)
      continue;
    buf[0] = toupper(buf[0]);
    for (n = 0; n < MAX_FIELDS; n++)						/* Reset fields */
      field[n] = (uchar *)NULL;
    for (n = 0, b = buf + 1; n < MAX_FIELDS; n++)	/* Load fields */
      if (!(field[n] = (uchar*)strsep(&b, ":")))
	break;
    switch (buf[0]) {
    case '.':
      tinydns_dot();
      break;

    case '&':
      tinydns_amp();
      break;

    case '=':
      tinydns_equal();
      break;

    case '+':
      tinydns_plus();
      break;

    case '@':
      tinydns_at();
      break;

    case '#':		/* Comment line */
      break;

    case '-':		/* Ignored (by tinydns-data) */
      break;

    case '\'':
      tinydns_apos();
      break;

    case '^':
      tinydns_caret();
      break;

    case 'C':
      tinydns_C();
      break;

    case 'Z':		/* Ignored (SOA record; already processed on first pass) */
      break;
      
    case ':':
      tinydns_colon();
      break;

    default:
      Warnx("%s:%u: %s (0x%X)", filename, lineno, _("unknown line type"), buf[0]);
      break;
    }
  }
  fclose(fp);
  if (import_zone)
    Notice("%s: %s: %s", filename, import_zone, _("zone imported"));
}
/*--- import_tinydns() --------------------------------------------------------------------------*/

#endif /* TINYDNS */

/* vi:set ts=3: */
/* NEED_PO */
