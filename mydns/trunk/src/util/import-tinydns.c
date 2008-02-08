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

#define MAX_FIELDS	20											/* Max fields per line */
#define TINYDNS_DEF_REFRESH	16384
#define TINYDNS_DEF_RETRY		2048
#define TINYDNS_DEF_EXPIRE		1048576
#define TINYDNS_DEF_MINIMUM	2560
#define TINYDNS_DEF_TTL			2560

typedef struct _zone
{
	uint32_t id;
	char		*origin, *ns, *mbox;
	uint32_t	serial, refresh, retry, expire, minimum, ttl;
} ZONE;

static ZONE	**Zones = NULL;									/* List of zones found */
static int	numZones = 0;										/* Number of zones found */

static uint32_t		default_serial;						/* Default serial */
static unsigned int	lineno;									/* Current line number */
static char				*filename;								/* Input file name */
static char				*field[MAX_FIELDS];					/* Fields parsed from line */

static char				tinydns_zone[DNS_MAXNAMELEN+1];	/* Zone to import (only this zone) */

extern uint32_t		import_zone_id;						/* ID of current zone */

extern uint32_t import_soa(const char *origin, const char *ns, const char *mbox,
  unsigned serial, unsigned refresh, unsigned retry, unsigned expire,
  unsigned minimum, unsigned ttl);
extern void import_rr(char *name, char *type, char *data, unsigned aux, unsigned ttl);


/**************************************************************************************************
	TWARN
	Convenience function; outputs error message for current file/line.
**************************************************************************************************/
static void
twarn(const char *fmt, ...)
{
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
zone_ok(const char *fqdn)
{
	if (!tinydns_zone[0])										/* Processing all zones */
		return 1;
	if (!strcasecmp(fqdn, tinydns_zone))					/* Exact match on zone */
		return 1;

	/* Does FQDN at least end with the zone? */
	if (strlen(fqdn) > strlen(tinydns_zone)
		 && !strcasecmp(fqdn + strlen(fqdn) - strlen(tinydns_zone), tinydns_zone))
		return 1;

	return 0;
}
/*--- zone_ok() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	ZONECMP
	Comparison function for sorting zones.
**************************************************************************************************/
static int
zonecmp(const void *p1, const void *p2)
{
	ZONE *z1 = *(ZONE **)p1, *z2 = *(ZONE **)p2;
	return strcasecmp(z1->origin, z2->origin);
}
/*--- zonecmp() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	FIND_ZONE
	Looks for the specified zone.  Returns it if found, else NULL.
**************************************************************************************************/
static inline ZONE *
find_zone(const char *origin)
{
	register int n;

	for (n = 0; n < numZones; n++)
		if (!strcasecmp(Zones[n]->origin, origin))
			return Zones[n];
	return NULL;
}
/*--- find_zone() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	FIND_HOST_ZONE
	Looks for the zone matching the provided FQDN.  Returns it if found, else NULL.  Also sets
	the hostname part of 'fqdn' in 'hostname'.
**************************************************************************************************/
static inline ZONE *
find_host_zone(char *fqdn, char *hostname)
{
	register char *c;
	ZONE *z;

	hostname[0] = '\0';

	if ((z = find_zone(fqdn)))									/* See if 'fqdn' is a plain zone origin */
		return (z);

	for (c = fqdn; *c; c++)
		if (*c == '.')
			if ((z = find_zone(c + 1)))
			{
				memcpy(hostname, fqdn, c - fqdn);
				hostname[c - fqdn] = '\0';
				return (z);
			}

	return NULL;
}
/*--- find_host_zone() --------------------------------------------------------------------------*/


/**************************************************************************************************
	FIND_ARPA_ZONE
	Given an IP address in numbers-and-dots format, returns the relevant in-addr.arpa zone, or
	NULL if not found.
**************************************************************************************************/
static inline ZONE *
find_arpa_zone(const char *ip, char *hostname)
{
	char zone[DNS_MAXNAMELEN + 1], buf[DNS_MAXNAMELEN + 1], *p, *a, *b, *c, *d;
	ZONE *z;

	hostname[0] = '\0';
	strncpy(buf, ip, sizeof(buf)-1);
	p = buf;
	if (!(a = strsep(&p, ".")) || !(b = strsep(&p, "."))
		 || !(c = strsep(&p, ".")) || !(d = strsep(&p, ".")))
		return NULL;

	snprintf(zone, sizeof(zone), "%s.in-addr.arpa", a);
	if ((z = find_zone(zone)))
	{
		snprintf(hostname, DNS_MAXNAMELEN, "%s.%s.%s", d, c, b);
		return z;
	}

	snprintf(zone, sizeof(zone), "%s.%s.in-addr.arpa", b, a);
	if ((z = find_zone(zone)))
	{
		snprintf(hostname, DNS_MAXNAMELEN, "%s.%s", d, c);
		return z;
	}

	snprintf(zone, sizeof(zone), "%s.%s.%s.in-addr.arpa", c, b, a);
	if ((z = find_zone(zone)))
	{
		strncpy(hostname, d, DNS_MAXNAMELEN);
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
new_zone(const char *originp)
{
	ZONE *z;
	char origin[DNS_MAXNAMELEN+1];

	/* If this is an 'in-addr.arpa' zone, knock off the first quad if present */
	if (strlen(originp) > 12 && !strcasecmp(originp + strlen(originp) - 13, ".in-addr.arpa"))
	{
		char buf[DNS_MAXNAMELEN+1], *p, *a, *b, *c, *d;

		strncpy(buf, originp, sizeof(buf)-1);
		p = buf;
		if ((d = strsep(&p, ".")) && (c = strsep(&p, "."))
			 && (b = strsep(&p, ".")) && (a = strsep(&p, ".")))
			snprintf(origin, sizeof(origin), "%s.%s.%s.in-addr.arpa", c, b, a);
		else
			strncpy(origin, originp, sizeof(origin)-1);
	}
	else
		strncpy(origin, originp, sizeof(origin)-1);

	if ((z = find_zone(origin)))
		return z;

	if (!(z = malloc(sizeof(ZONE))))
		Err("malloc");

	z->id = 0;
	if (!(z->origin = strdup(origin)))
		Err("strdup");
	z->ns = NULL;
	z->mbox = NULL;
	z->serial = default_serial;
	z->refresh = TINYDNS_DEF_REFRESH;
	z->retry = TINYDNS_DEF_RETRY;
	z->expire = TINYDNS_DEF_EXPIRE;
	z->minimum = TINYDNS_DEF_MINIMUM;
	z->ttl = TINYDNS_DEF_TTL;

	if (!Zones)
	{
		if (!(Zones = malloc((numZones + 1) * sizeof(ZONE))))
			Err("malloc");
	}
	else
	{
		if (!(Zones = realloc(Zones, (numZones + 1) * sizeof(ZONE))))
			Err("realloc");
	}
	Zones[numZones++] = z;

	return z;
}
/*--- new_zone() --------------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_ADD_SOA
	Adds a zone to 'Zones' due to a '.' or 'Z' line.
**************************************************************************************************/
void
tinydns_add_soa(char *line)
{
	char	*l = line + 1;
	ZONE	*z;

	if (line[0] == '.')											/* '.' (NS) line */
	{
		char *fqdn = NULL, *ip = NULL, *x = NULL, *ttl = NULL, *timestamp = NULL, *lo = NULL;

		if (!(fqdn = strsep(&l, ":")) || !strlen(fqdn))
			return twarn("'.' line has no fqdn");

		if (!zone_ok(fqdn))
			return;

		if (!(ip = strsep(&l, ":")))
			return twarn("'.' line has no ip");

		if ((x = strsep(&l, ":")) && (ttl = strsep(&l, ":")) && (timestamp = strsep(&l, ":"))
			 && (lo = strsep(&l, ":")))
			/* DONOTHING */;

		z = new_zone(fqdn);

		/* Assign 'ns' */
		if (!z->ns)
		{
			if (!x || !strlen(x))
				sdprintf(&z->ns, "ns.%s", fqdn);
			else if (strchr(x, '.'))
			{
				if (!(z->ns = strdup(x)))
					Err("strdup");
			}
			else
				sdprintf(&z->ns, "%s.ns.%s", x, fqdn);
		}

		/* Assign 'mbox' */
		if (!z->mbox)
			sdprintf(&z->mbox, "hostmaster.%s", fqdn);
	}
	else if (line[0] == 'Z')									/* 'Z' (SOA) line */
	{
		char *fqdn = NULL, *ns = NULL, *mbox = NULL, *serial = NULL, *refresh = NULL,
			  *retry = NULL, *expire = NULL, *minimum = NULL, *ttl = NULL, *timestamp = NULL,
			  *lo = NULL;

		if (!(fqdn = strsep(&l, ":")) || !strlen(fqdn))
			return twarn("'Z' line has no fqdn");

		if (!zone_ok(fqdn))
			return;

		if (!(ns = strsep(&l, ":")) || !strlen(ns))
			return twarn("'Z' line has no ns");
		if (!(mbox = strsep(&l, ":")) || !strlen(mbox))
			return twarn("'Z' line has no mbox");

		if ((serial = strsep(&l, ":")) && (refresh = strsep(&l, ":")) && (retry = strsep(&l, ":"))
			 && (expire = strsep(&l, ":")) && (minimum = strsep(&l, ":")) && (ttl = strsep(&l, ":"))
			 && (timestamp = strsep(&l, ":")) && (lo = strsep(&l, ":")))
			/* DONOTHING */;

		z = new_zone(fqdn);
		if (!z->ns)
		{
			if (!(z->ns = strdup(ns)))
				Err("strdup");
		}
		if (!z->mbox)
		{
			if (!(z->mbox = strdup(mbox)))
				Err("strdup");
		}
		if (serial) z->serial = atol(serial);
		if (refresh) z->refresh = atol(refresh);
		if (retry) z->retry = atol(retry);
		if (expire) z->expire = atol(expire);
		if (minimum) z->minimum = atol(minimum);
		if (ttl) z->ttl = atol(ttl);
	}
}
/*--- tinydns_add_soa() -------------------------------------------------------------------------*/


/**************************************************************************************************
	CREATE_ZONE
	Loads or creates the actual SOA record and loads the ID.
**************************************************************************************************/
static void
create_zone(ZONE *z)
{
	char origin[DNS_MAXNAMELEN + 3], ns[DNS_MAXNAMELEN + 3], mbox[DNS_MAXNAMELEN + 3];

	if (!z->ns)
		sdprintf(&z->ns, "ns.%s", z->origin);
	if (!z->mbox)
		sdprintf(&z->mbox, "hostmaster.%s", z->origin);

	snprintf(origin, sizeof(origin), "%s.", z->origin);
	snprintf(ns, sizeof(ns), "%s.", z->ns);
	snprintf(mbox, sizeof(mbox), "%s.", z->mbox);

	z->id = import_soa(origin, ns, mbox,
							 z->serial, z->refresh, z->retry, z->expire, z->minimum, z->ttl);
}
/*--- create_zone() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_PLUS

	+fqdn:ip:ttl:timestamp:lo

	Alias fqdn with IP address ip. This is just like =fqdn:ip:ttl except that tinydns-data
	does not create the PTR record.
**************************************************************************************************/
static void
tinydns_plus(void)
{
	ZONE	*z;
	char	*fqdn = field[0], *ip = field[1], *ttl = field[2];
	char	hostname[DNS_MAXNAMELEN + 1] = "";

	if (!fqdn || !strlen(fqdn))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
	if (!zone_ok(fqdn))
		return;
	if (!(z = find_host_zone(fqdn, hostname)))
		return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));

	/* Set import_zone_id and import the record */
	if (ip && strlen(ip))
	{
		import_zone_id = z->id;
		import_rr(hostname, "A", ip, 0, ttl ? atol(ttl) : z->ttl);
	}
}
/*--- tinydns_plus() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_DOT

	.fqdn:ip:x:ttl:timestamp:lo

	1. Create an NS record showing x.ns.fqdn as a name server for fqdn
	2. Create an A record showing ip as the IP address of x.ns.fqdn
**************************************************************************************************/
static void
tinydns_dot(void)
{
	ZONE	*z;
	char	*fqdn = field[0], *ip = field[1], *x = field[2], *ttl = field[3];
	char	hostname[DNS_MAXNAMELEN + 1] = "", buf[DNS_MAXNAMELEN + 1];

	if (!fqdn || !strlen(fqdn))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
	if (!zone_ok(fqdn))
		return;

	if (!(z = find_host_zone(fqdn, hostname)))
		return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
	import_zone_id = z->id;

	/* Create NS record */
	if (x && strlen(x))
	{
		if (strchr(x, '.'))
			strncpy(buf, x, sizeof(buf)-1);
		else
			snprintf(buf, sizeof(buf), "%s.ns", x);
		if (LASTCHAR(buf) != '.')
			strcat(buf, ".");
		import_rr(hostname, "NS", buf, 0, ttl ? atol(ttl) : z->ttl);
	}
	else
	{
		strncpy(buf, "ns", sizeof(buf)-1);
		import_rr(hostname, "NS", buf, 0, ttl ? atol(ttl) : z->ttl);
	}

	/* Create A record if 'ip' is present */
	if (ip && strlen(ip))
		import_rr(buf, "A", ip, 0, ttl ? atol(ttl) : z->ttl);
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
tinydns_amp(void)
{
	ZONE	*z;
	char	*fqdn = field[0], *ip = field[1], *x = field[2], *ttl = field[3];
	char	hostname[DNS_MAXNAMELEN + 1] = "", buf[DNS_MAXNAMELEN + 1];

	if (!fqdn || !strlen(fqdn))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
	if (!zone_ok(fqdn))
		return;

	if (!(z = find_host_zone(fqdn, hostname)))
		return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
	import_zone_id = z->id;

	/* Create NS record */
	if (x && strlen(x))
	{
		if (strchr(x, '.'))
			strncpy(buf, x, sizeof(buf)-1);
		else
			snprintf(buf, sizeof(buf), "%s.ns", x);
		if (LASTCHAR(buf) != '.')
			strcat(buf, ".");
		import_rr(hostname, "NS", buf, 0, ttl ? atol(ttl) : z->ttl);
	}
	else
	{
		strncpy(buf, "ns", sizeof(buf)-1);
		import_rr(hostname, "NS", buf, 0, ttl ? atol(ttl) : z->ttl);
	}

	/* Create A record if 'ip' is present */
	if (ip && strlen(ip))
		import_rr(buf, "A", ip, 0, ttl ? atol(ttl) : z->ttl);
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
tinydns_equal(void)
{
	ZONE	*z;
	char	*fqdn = field[0], *ip = field[1], *ttl = field[2];
	char	hostname[DNS_MAXNAMELEN + 1] = "";

	if (!fqdn || !strlen(fqdn))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
	if (!zone_ok(fqdn))
		return;

	if (!(z = find_host_zone(fqdn, hostname)))
		return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
	import_zone_id = z->id;

	/* Create A record showing 'ip' as the IP address of 'fqdn' */
	import_rr(fqdn, "A", ip, 0, ttl ? atol(ttl) : z->ttl);

	/* Create PTR record if we can find an appropriate in-addr.arpa zone */
	if ((z = find_arpa_zone(ip, hostname)))
	{
		import_zone_id = z->id;
		import_rr(hostname, "PTR", fqdn, 0, ttl ? atol(ttl) : z->ttl);
	}
	else
	{
		char buf[DNS_MAXNAMELEN + 1], *p, *a, *b, *c, *d;

		strncpy(buf, ip, sizeof(buf)-1);
		p = buf;
		if (!(a = strsep(&p, ".")) || !(b = strsep(&p, "."))
			 || !(c = strsep(&p, ".")) || !(d = strsep(&p, ".")))
			return;
		Warnx("%s:%u: %s.%s.%s.%s.in-addr.arpa: %s", filename, lineno,
			d, c, b, a, _("no matching zone for PTR record"));
	}
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
tinydns_at(void)
{
	ZONE	*z;
	char	*fqdn = field[0], *ip = field[1], *x = field[2], *dist = field[3], *ttl = field[4];
	char	hostname[DNS_MAXNAMELEN + 1] = "", mx[DNS_MAXNAMELEN + 1] = "";

	if (!fqdn || !strlen(fqdn))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
	if (!zone_ok(fqdn))
		return;

	if (!x || !strlen(x))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("no mail exchanger specified"));

	if (!(z = find_host_zone(fqdn, hostname)))
		return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
	import_zone_id = z->id;

	/* Create MX record showing 'x'.mx.fqdn as MX for 'fqdn' */
	if (strchr(x, '.'))
		strncpy(mx, x, sizeof(mx)-1);
	else
		snprintf(mx, sizeof(mx), "%s.mx.%s", x, z->origin);
	if (LASTCHAR(mx) != '.')
		strcat(mx, ".");
	import_rr(hostname, "MX", mx, dist ? atol(dist) : 0, ttl ? atol(ttl) : z->ttl);

	/* Create A record for 'mx' */
	if (ip && strlen(ip))
	{
		if (!(z = find_host_zone(mx, hostname)))
			return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
		import_zone_id = z->id;
		import_rr(mx, "A", ip, 0, ttl ? atol(ttl) : z->ttl);
	}
}
/*--- tinydns_at() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_APOS

	'fqdn:str:ttl:timestamp:lo

	Creates a TXT record for 'fqdn' containing the string 'str'.  You may use octal \nnn codes to
	include arbitrary bytes inside 'str'; for example, \072 is a colon.
**************************************************************************************************/
static void
tinydns_apos(void)
{
	ZONE	*z;
	char	*fqdn = field[0], *str = field[1], *ttl = field[2], *txt;
	char	hostname[DNS_MAXNAMELEN + 1] = "";
	register char *s, *d;

	if (!fqdn || !strlen(fqdn))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
	if (!zone_ok(fqdn))
		return;

	if (!str || !strlen(str))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("no text data specified"));

	if (!(z = find_host_zone(fqdn, hostname)))
		return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
	import_zone_id = z->id;

	/* Unescape octal chars */
	if (!(txt = malloc(strlen(str) + 1)))
		Err("malloc");
	for (s = str, d = txt; *s; s++)
	{
		if (s[0] == '\\' && (s[1] >= '0' && s[1] <= '7')
			 && (s[2] >= '0' && s[2] <= '7') && (s[3] >= '0' && s[3] <= '7'))
		{
			*d++ = strtol(s + 1, NULL, 8);
			s += 3;
		}
		else
			*d++ = *s;
	}
	*d = '\0';
	import_rr(hostname, "TXT", txt, 0, ttl ? atol(ttl) : z->ttl);
}
/*--- tinydns_apos() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_CARET

	^fqdn:p:ttl:timestamp:lo

	PTR record for fqdn. tinydns-data creates a PTR record for fqdn pointing to the domain name p.
**************************************************************************************************/
static void
tinydns_caret(void)
{
	ZONE	*z;
	char	*fqdn = field[0], *p = field[1], *ttl = field[2];
	char	hostname[DNS_MAXNAMELEN + 1] = "", ptr[DNS_MAXNAMELEN + 1] = "";

	if (!fqdn || !strlen(fqdn))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
	if (!zone_ok(fqdn))
		return;

	if (!p || !strlen(p))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("no PTR data specified"));
	strncpy(ptr, p, sizeof(ptr)-1);
	if (strchr(ptr, '.') && LASTCHAR(ptr) != '.')
		strcat(ptr, ".");

	if (!(z = find_host_zone(fqdn, hostname)))
		return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
	import_zone_id = z->id;
	import_rr(hostname, "PTR", ptr, 0, ttl ? atol(ttl) : z->ttl);
}
/*--- tinydns_caret() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	TINYDNS_C

	Cfqdn:p:ttl:timestamp:lo

	Creates a CNAME record for 'fqdn' pointing to the domain name 'p'.
**************************************************************************************************/
static void
tinydns_C(void)
{
	ZONE	*z;
	char	*fqdn = field[0], *p = field[1], *ttl = field[2];
	char	hostname[DNS_MAXNAMELEN + 1] = "", cname[DNS_MAXNAMELEN + 1] = "";

	if (!fqdn || !strlen(fqdn))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
	if (!zone_ok(fqdn))
		return;

	if (!p || !strlen(p))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("no PTR data specified"));
	strncpy(cname, p, sizeof(cname)-1);
	if (strchr(cname, '.') && LASTCHAR(cname) != '.')
		strcat(cname, ".");

	if (!(z = find_host_zone(fqdn, hostname)))
		return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));
	import_zone_id = z->id;
	import_rr(hostname, "CNAME", cname, 0, ttl ? atol(ttl) : z->ttl);
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
tinydns_colon(void)
{
	ZONE	*z;
	char	*fqdn = field[0]; /* *n = field[1], *rdata = field[2], *ttl = field[3]; */
	char	hostname[DNS_MAXNAMELEN + 1] = "";

	if (!fqdn || !strlen(fqdn))
		return (void)Warnx("%s:%u: %s", filename, lineno, _("fqdn field empty"));
	if (!zone_ok(fqdn))
		return;

	if (!(z = find_host_zone(fqdn, hostname)))
		return (void)Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("fqdn does not match any zones"));

	Warnx("%s:%u: %s: %s", filename, lineno, fqdn, _("generic record type not supported"));
}
/*--- tinydns_colon() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	IMPORT_TINYDNS
	Imports a tinydns-data format file.  Works in two passes: The first pass creates all SOA
	records, the second pass creates all RR records.
**************************************************************************************************/
void
import_tinydns(char *datafile, char *import_zone)
{
	struct stat st;
	FILE	*fp;
	char	buf[4096], *b;
	int	n;

	/* Clear zone list */
	for (n = 0; n < numZones; n++)
	{
		Free(Zones[n]->origin);
		if (Zones[n]->ns)
			Free(Zones[n]->ns);
		if (Zones[n]->mbox)
			Free(Zones[n]->mbox);
		Free(Zones[n]);
	}
	if (Zones)
		Free(Zones);
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
	while (fgets(buf, sizeof(buf), fp))
	{
		lineno++;
		strtrim(buf);
		if (strlen(buf) < 2)
			continue;
		buf[0] = toupper(buf[0]);
		if (buf[0] == '.' || buf[0] == 'Z')
			tinydns_add_soa(buf);
	}
	if (!numZones)
	{
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
	while (fgets(buf, sizeof(buf), fp))
	{
		lineno++;
		strtrim(buf);
		if (strlen(buf) < 2)
			continue;
		buf[0] = toupper(buf[0]);
		for (n = 0; n < MAX_FIELDS; n++)						/* Reset fields */
			field[n] = (char *)NULL;
		for (n = 0, b = buf + 1; n < MAX_FIELDS; n++)	/* Load fields */
			if (!(field[n] = strsep(&b, ":")))
				break;
		switch (buf[0])
		{
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
