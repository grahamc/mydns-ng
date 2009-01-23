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

#include "memoryman.h"

#include "bits.h"
#include "import.h"
#include "support.h"

uint32_t got_soa = 0;					/* Have we read the initial SOA record? */
int opt_output = 0;					/* Output instead of insert */
int opt_notrim = 0;					/* Don't remove trailing origin */
int opt_replace = 0;					/* Replace current zone */
int IXFR = 0;						/* Serial number on the rr records */
char *ACTIVE = NULL;					/* ACTIVE String to use */


uint32_t	import_zone_id = 0;			/* ID of current zone */
uint32_t	import_serial = 0;			/* Serial number of current zone */

int	soa_imported, rr_imported, ptr_imported;	/* Number of records imported */

/**************************************************************************************************
	IMPORT_SOA
	Update, replace, or output SOA.  Returns the zone ID or 1 if not working with database.
**************************************************************************************************/
uint32_t import_soa(const char *import_origin, const char *ns, const char *mbox,
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
#if DEBUG_ENABLED
    DebugX("import-sql", 1, "DELETE FROM %s WHERE zone=%u;", mydns_rr_table_name, import_zone_id);
#endif
    sql_queryf(sql, "DELETE FROM %s WHERE zone=%u;", mydns_rr_table_name, import_zone_id);

    /* Update "soa" table */
#if DEBUG_ENABLED
    DebugX("import-sql", 1,
	   "UPDATE %s SET origin='%s',ns='%s',mbox='%s',serial=%u,refresh=%u,retry=%u,"
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
#if DEBUG_ENABLED
    DebugX("import-sql", 1, "INSERT INTO %s (origin,ns,mbox,serial,refresh,retry,expire,minimum,ttl)"
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
import_rr(char *name, char *type, char *data, int datalen, unsigned aux, unsigned ttl) {
  char *esc_name, *esc_data, *esc_edata = NULL;
  char *querystr = NULL;
  char *edata = NULL;
  size_t edatalen = 0;


  if (!name || !type || !data)
    Errx(_("Invalid resource record name=%s, type=%s, data=%p"),
	 (name)?name:"<undef>", (type)?type:"<undef>", data);

  rr_imported++;
  Verbose("import rr \"%s\" %s \"%s\"", name, type, data);

  if (opt_output) {
    printf("rr\t%s\t%s\t%s\t%u\t%u\n", name, type, data, aux, ttl);
    return;
  }
  if (import_zone_id < 1)
    Errx(_("got rr record without soa"));

  /* s = shortname(name); */
  esc_name = sql_escstr(sql, name);
  if (mydns_rr_extended_data && datalen > mydns_rr_data_length) {
    edatalen = datalen - mydns_rr_data_length;
    edata = &data[mydns_rr_data_length];
    datalen = mydns_rr_data_length;
    esc_edata = sql_escstr2(sql, edata, edatalen);
  }
  esc_data = sql_escstr2(sql, data, datalen);
  if (IXFR) {
    querystr = "INSERT INTO %s (zone,name,type,data%s,aux,ttl,active,serial) "
      "VALUES (%u,'%s','%s','%s'%s%s%s,%u,%u,'%s',%u);";
#if DEBUG_ENABLED
    DebugX("import-sql", 1, querystr, mydns_rr_table_name, (edatalen)?",edata":"",
	   import_zone_id, esc_name, type, esc_data,
	   (edatalen)?",'":"",
	   (edatalen)?esc_edata:"",
	   (edatalen)?"'":"",
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
#if DEBUG_ENABLED
    DebugX("import-sql", 1, querystr, mydns_rr_table_name, (edatalen)?",edata":"",
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
#if DEBUG_ENABLED
    DebugX("import-sql", 1, querystr, mydns_rr_table_name, (edatalen)?",edata":"",
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

char *__mydns_process_axfr_soa(char *rv, char *name, char *origin,
			       char *reply, size_t replylen, char *src, uint32_t ttl,
			       dns_qtype_map *map) {
  char *ns, *mbox;
  task_error_t errcode;
  uint32_t serial, refresh, retry, expire, minimum;

  if (got_soa) return NULL;

  if (!(ns = name_unencode(reply, replylen, &src, &errcode)))
    Errx("%s SOA: %s: %s", name , _("error reading ns from SOA"), name);
  if (!(mbox = name_unencode(reply, replylen, &src, &errcode)))
    Errx("%s SOA: %s: %s", name, _("error reading mbox from SOA"), name);
  DNS_GET32(serial, src);
  DNS_GET32(refresh, src);
  DNS_GET32(retry, src);
  DNS_GET32(expire, src);
  DNS_GET32(minimum, src);
  if (ttl < minimum)
    ttl = minimum;
  origin = STRDUP(name);
  got_soa = import_soa(origin, ns, mbox, serial, refresh, retry, expire, minimum, ttl);
  RELEASE(ns);
  RELEASE(mbox);

  return rv;
}

char *__mydns_process_axfr_a(char *rv, char *name, char *origin,
			     char *reply, size_t replylen, char *src, uint32_t ttl,
			     dns_qtype_map *map) {
  char *data;
  struct in_addr addr;
  memcpy(&addr.s_addr, src, SIZE32);
  data = (char*)ipaddr(AF_INET, &addr);
  import_rr(mydns_name_2_shortname(name, origin, 1, opt_notrim), "A", data, strlen(data), 0, ttl);
  return rv;

}

char *__mydns_process_axfr_aaaa(char *rv, char *name, char *origin,
				char *reply, size_t replylen, char *src, uint32_t ttl,
				dns_qtype_map *map) {
  char *data;
  uint8_t addr[16]; /* This is a cheat.
		    ** it should be a 'struct in6_addr'
		    ** but we can't be sure it will exist */

  memcpy(&addr, src, sizeof(addr));
  if ((data = (char*)ipaddr(AF_INET6, &addr))) {
    import_rr(mydns_name_2_shortname(name, origin, 1, opt_notrim), "AAAA", data, strlen(data), 0, ttl);
  } else {
    Notice("%s IN AAAA: %s", name, strerror(errno));
  }
  return rv;
}

char *__mydns_process_axfr_cname(char *rv, char *name, char *origin,
				 char *reply, size_t replylen, char *src, uint32_t ttl,
				 dns_qtype_map *map) {
  char *data;
  task_error_t errcode;
  if (!(data = name_unencode(reply, replylen, &src, &errcode)))
    Errx("%s CNAME: %s: %s", name, _("error reading data"), data);
  import_rr(mydns_name_2_shortname(name, origin, 1, opt_notrim), "CNAME",
	    mydns_name_2_shortname(data, origin, 0, opt_notrim),
	    strlen(mydns_name_2_shortname(data, origin, 0, opt_notrim)), 0, ttl);
  RELEASE(data);
  return rv;
}

char *__mydns_process_axfr_hinfo(char *rv, char *name, char *origin,
				 char *reply, size_t replylen, char *src, uint32_t ttl,
				 dns_qtype_map *map) {
  char *data;
  size_t	len;
  int		quote1, quote2;
  char		*c, *data2;
  char		*insdata;

  len = *src++;
  data = ALLOCATE(len+1, char[]);
  memcpy(data, src, len);
  data[len] = '\0';
  src += len;
  for (c = data, quote1 = 0; *c; c++)
    if (!isalnum(*c))
      quote1++;

  len = *src++;
  data2 = ALLOCATE(len+1, char[]);
  memcpy(data2, src, len);
  data2[len] = '\0';
  src += len;
  for (c = data2, quote2 = 0; *c; c++)
    if (!isalnum(*c))
      quote2++;

  ASPRINTF(&insdata, "%s%s%s %s%s%s",
	   quote1 ? "\"" : "", data, quote1 ? "\"" : "",
	   quote2 ? "\"" : "", data2, quote2 ? "\"" : "");
  RELEASE(data);
  RELEASE(data2);
  import_rr(mydns_name_2_shortname(name, origin, 1, opt_notrim), "HINFO", insdata, strlen(insdata), 0, ttl);
  RELEASE(insdata);
  return rv;
}

char *__mydns_process_axfr_mx(char *rv, char *name, char *origin,
			      char *reply, size_t replylen, char *src, uint32_t ttl,
			      dns_qtype_map *map) {
  char *data;
  task_error_t errcode;
  uint16_t pref;
  DNS_GET16(pref, src);
  if (!(data = name_unencode(reply, replylen, &src, &errcode)))
    Errx("%s MX: %s: %s", name, _("error reading data"), data);
  import_rr(mydns_name_2_shortname(name, origin, 1, opt_notrim), "MX",
	    mydns_name_2_shortname(data, origin, 0, opt_notrim),
	    strlen(mydns_name_2_shortname(data, origin, 0, opt_notrim)), pref, ttl);
  RELEASE(data);
  return rv;
}

char *__mydns_process_axfr_ns(char *rv, char *name, char *origin,
			      char *reply, size_t replylen, char *src, uint32_t ttl,
			      dns_qtype_map *map) {
  char *data;
  task_error_t errcode;
  if (!(data = name_unencode(reply, replylen, &src, &errcode)))
    Errx("%s NS: %s: %s", name, _("error reading data"), data);
  import_rr(mydns_name_2_shortname(name, origin, 1, opt_notrim), "NS",
	    mydns_name_2_shortname(data, origin, 0, opt_notrim),
	    strlen(mydns_name_2_shortname(data, origin, 0, opt_notrim)), 0, ttl);
  RELEASE(data);
  return rv;
}

char *__mydns_process_axfr_ptr(char *rv, char *name, char *origin,
			       char *reply, size_t replylen, char *src, uint32_t ttl,
			       dns_qtype_map *map) {
  char *data;
  task_error_t errcode;
  struct in_addr addr;
  addr.s_addr = mydns_revstr_ip4(name);
  if (!(data = name_unencode(reply, replylen, &src, &errcode)))
    Errx("%s PTR: %s: %s", name, _("error reading data"), data);
  import_rr(mydns_name_2_shortname(name, origin, 1, opt_notrim), "PTR",
	    mydns_name_2_shortname(data, origin, 0, opt_notrim),
	    strlen(mydns_name_2_shortname(data, origin, 0, opt_notrim)), 0, ttl);
  RELEASE(data);
  return rv;
}

char *__mydns_process_axfr_rp(char *rv, char *name, char *origin,
			      char *reply, size_t replylen, char *src, uint32_t ttl,
			      dns_qtype_map *map) {
  char *data;
  task_error_t errcode;
  char *txtref;
  char *insdata;
      
  /* Get mbox in 'data' */
  if (!(data = name_unencode(reply, replylen, &src, &errcode)))
    Errx("%s RP: %s: %s", name, _("error reading mbox"), data);
      
  /* Get txt in 'txtref' */
  if (!(txtref = name_unencode(reply, replylen, &src, &errcode)))
    Errx("%s RP: %s: %s", name, _("error reading txt"), txtref);
      
  /* Construct data to insert */
  ASPRINTF(&insdata, "%s %s", mydns_name_2_shortname(data, origin, 0, opt_notrim),
	   mydns_name_2_shortname(txtref, origin, 0, opt_notrim));
  RELEASE(data);
  RELEASE(txtref);
  import_rr(mydns_name_2_shortname(name, origin, 1, opt_notrim), "RP", insdata, strlen(insdata), 0, ttl);
  RELEASE(insdata);
  return rv;
}

char *__mydns_process_axfr_srv(char *rv, char *name, char *origin,
			       char *reply, size_t replylen, char *src, uint32_t ttl,
			       dns_qtype_map *map) {
  char *data;
  task_error_t errcode;
  uint16_t priority, weight, port;
  char		 *databuf;
      
  DNS_GET16(priority, src);
  DNS_GET16(weight, src);
  DNS_GET16(port, src);
  if (!(data = name_unencode(reply, replylen, &src, &errcode)))
    Errx("%s SRV: %s: %s", name, _("error reading data"), data);
  ASPRINTF(&databuf, "%u %u %s", weight, port, data);
  RELEASE(data);
  import_rr(mydns_name_2_shortname(name, origin, 1, opt_notrim), "SRV", databuf, strlen(databuf), priority, ttl);
  RELEASE(databuf);
  return rv;
}

char *__mydns_process_axfr_txt(char *rv, char *name, char *origin,
			       char *reply, size_t replylen, char *src, uint32_t ttl,
			       dns_qtype_map *map) {
  char *data;
  size_t len = *src++;
      
  data = ALLOCATE(len + 1, char[]);
  memcpy(data, src, len);
  data[len] = '\0';
  src += len;
  import_rr(mydns_name_2_shortname(name, origin, 1, opt_notrim), "TXT", data, len, 0, ttl);
  RELEASE(data);
  return rv;
}

char *__mydns_process_axfr_default(char *rv, char *name, char *origin,
				   char *reply, size_t replylen, char *src, uint32_t ttl,
				   dns_qtype_map *map) {
  Warnx("%s %s: %s", name, map->rr_type_name, _("discarding unsupported RR type"));
  return NULL;
}

/* vi:set ts=3: */
/* NEED_PO */
