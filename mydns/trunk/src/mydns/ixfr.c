/**************************************************************************************************
	$Id: notify.c,v 1.0 2007/09/04 10:00:57 howard Exp $

	Copyright (C) 2007 Howard Wilkinson <howard@cohtech.com>

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

/* Make this nonzero to enable debugging for this source file */
#define	DEBUG_IXFR	1

#define DEBUG_IXFR_SQL 1

typedef struct _ixfr_authority_rr {
  uchar			*name;
  dns_qtype_t		type;
  dns_class_t		class;
  uint32_t		ttl;
  uchar			*mname;
  uchar			*rname;
  uint32_t		serial;
  uint32_t		refresh;
  uint32_t		retry;
  uint32_t		expire;
  uint32_t		minimum;
} IARR;

#define IARR_NAME(__rrp)		((__rrp)->name)
#define IARR_MNAME(__rrp)		((__rrp)->mname)
#define IARR_RNAME(__rrp)		((__rrp)->rname)

typedef struct _ixfr_query {
  /* Zone section */
  uchar			*name;				/* The zone name */
  dns_qtype_t		type;				/* Must be DNS_QTYPE_SOA */
  dns_class_t		class;				/* The zone's class */

  IARR			IR;
} IQ;

#define IQ_NAME(__iqp)			((__iqp)->name)

static IQ *
allocate_iq(void) {
  IQ *q = ALLOCATE(sizeof(IQ), IQ);

  memset(q, 0, sizeof(IQ));

  return q;
}

static void
free_iarr_data(IARR *rr) {
  RELEASE(IARR_NAME(rr));
  RELEASE(IARR_MNAME(rr));
  RELEASE(IARR_RNAME(rr));
}

static void
free_iq(IQ *q) {
  free_iarr_data(&q->IR);

  RELEASE(IQ_NAME(q));

  RELEASE(q);
}

static uchar *
ixfr_gobble_authority_rr(TASK *t, uchar *query, size_t querylen, uchar *current, IARR *rr){
  uchar * src = current;
  int rdlength = 0;
  task_error_t errcode = TASK_FAILED;

  if (!(IARR_NAME(rr) = name_unencode2(query, querylen, &src, &errcode))) {
    formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
    return NULL;
  }
  DNS_GET16(rr->type, src);
  DNS_GET16(rr->class, src);
  DNS_GET32(rr->ttl, src);

  DNS_GET16(rdlength, src);
  if (!(IARR_MNAME(rr) = name_unencode2(query, querylen, &src, &errcode))) {
    formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
    return NULL;
  }

  if (!(IARR_RNAME(rr) = name_unencode2(query, querylen, &src, &errcode))) {
    formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
    return NULL;
  }

  DNS_GET32(rr->serial, src);
  DNS_GET32(rr->refresh, src);
  DNS_GET32(rr->retry, src);
  DNS_GET32(rr->expire, src);
  DNS_GET32(rr->minimum, src);

  return src;
}

taskexec_t
ixfr(TASK * t, datasection_t section, dns_qtype_t qtype, char *fqdn, int truncateonly) {
  MYDNS_SOA	*soa = NULL;
  uchar		*query = (uchar*)t->query;
  int		querylen = t->len;
  uchar		*src = query + DNS_HEADERSIZE;
  IQ		*q = NULL;
  task_error_t	errcode = 0;

#if DEBUG_ENABLED && DEBUG_IXFR
  DebugX("ixfr", 1, "%s: ixfr(%s, %s, \"%s\", %d)", desctask(t),
	 resolve_datasection_str[section], mydns_qtype_str(qtype), fqdn, truncateonly);
#endif

  if (!dns_ixfr_enabled) {
    dnserror(t, DNS_RCODE_REFUSED, ERR_IXFR_NOT_ENABLED);
    return (TASK_FAILED);
  }

  /*
   * Authority section contains the SOA record for the client's version of the zone
   * only trust the serial number.
   */

  if (mydns_soa_load(sql, &soa, fqdn) < 0) {
    dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
    return (TASK_FAILED);
  }

  if (!soa) {
    dnserror(t, DNS_RCODE_REFUSED, ERR_ZONE_NOT_FOUND);
    return (TASK_FAILED);
  }

#if DEBUG_ENABLED && DEBUG_IXFR
  DebugX("ixfr", 1, _("%s: DNS IXFR: SOA id %u"), desctask(t), soa->id);
  DebugX("ixfr", 1, _("%s: DNS IXFR: QDCOUNT=%d (Query)"), desctask(t), t->qdcount);
  DebugX("ixfr", 1, _("%s: DNS IXFR: ANCOUNT=%d (Answer)"), desctask(t), t->ancount);
  DebugX("ixfr", 1, _("%s: DNS IXFR: AUCOUNT=%d (Authority)"), desctask(t), t->nscount);
  DebugX("ixfr", 1, _("%s: DNS IXFR: ADCOUNT=%d (Additional data)"), desctask(t), t->arcount);
#endif
  if (!t->nscount)
    return formerr(t, DNS_RCODE_FORMERR, ERR_NO_AUTHORITY,
		   _("ixfr query contains no authority data"));

  if (t->nscount != 1)
    return formerr(t, DNS_RCODE_FORMERR, ERR_MULTI_AUTHORITY,
		   _("ixfr query contains multiple authority records"));

  if (!t->qdcount)
    return formerr(t, DNS_RCODE_FORMERR, ERR_NO_QUESTION,
		   _("ixfr query does not contain question"));

  if (t->qdcount != 1)
    return formerr(t, DNS_RCODE_FORMERR, ERR_MULTI_QUESTIONS,
		   _("ixfr query contains multiple questions"));

  if (t->ancount || t->arcount)
    return formerr(t, DNS_RCODE_FORMERR, ERR_MALFORMED_REQUEST,
		   _("ixfr query has answer or additional data"));

  q = allocate_iq();

  if (!(IQ_NAME(q) = name_unencode2(query, querylen, &src, &errcode))) {
    free_iq(q);
    return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
  }

  DNS_GET16(q->type, src);
  DNS_GET16(q->class, src);

  if (!(src = ixfr_gobble_authority_rr(t, query, querylen, src, &q->IR))) {
    free_iq(q);
    return (TASK_FAILED);
  }

  /* Get the serial number from the RR record in the authority section */
#if DEBUG_ENABLED && DEBUG_IXFR
  DebugX("ixfr", 1, _("%s: DNS IXFR Question[zone %s qclass %s qtype %s]"
		      " Authority[zone %s qclass %s qtype %s ttl %u "
		      "mname %s rname %s serial %u refresh %u retry %u expire %u minimum %u]"),
	 desctask(t), q->name, mydns_class_str(q->class), mydns_qtype_str(q->type),
	 q->IR.name, mydns_class_str(q->IR.class), mydns_qtype_str(q->IR.type), q->IR.ttl,
	 q->IR.mname, q->IR.rname, q->IR.serial, q->IR.refresh, q->IR.retry, q->IR.expire, q->IR.minimum);
#endif

  /*
   * As per RFC 1995 we have 3 options for a response if a delta exists.
   *
   * We can send a full zone transfer if it will fit in a UDP packet and is smaller
   * than sending deltas
   *
   * We can send a delta transfer if it will fit into a single UDP packet and we can calculate
   * one for the difference between the client and the current serial
   *
   * We can send a packet with a single SOA record for the latest SOA. This will force the client
   * to initiate an AXFR.
   *
   * We can calculate the size of the response by either building both messages
   * or by an estimation technique. In either case we need to look at the data.
   *
   * I have chosen to check for altered records within the database first.
   *
   * First check is to make sure that the serial held by the client is not the current one
   *
   * Next check to see if out incremental data for the transition from client serial
   * to current serial has not expired.
   *
   * Then retrieve the updated records between the client serial and the latest serial.
   * and retrieve the entire zone ... a record count is the first check.
   *
   * If the number of delta records is larger than the number of zone records then send the zone
   *
   * Calculate the size of the variable parts of the record and compare.
   * We assume that name encoding will have an equal effect on the data.
   * So having chosen to send either the zone or the deltas construct the packet.
   *
   * Check that the packet has not overflowed the UDP limit and send. If it has
   * that abandon the packet and send one containing just the latest SOA.
   *
   */

  if (soa->serial == q->IR.serial) {
    /* Tell the client to do no zone transfer */
    rrlist_add(t, ANSWER, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
    t->sort_level++;
  } else {
    /* Do we have incremental information in the database */
    if (!truncateonly && mydns_rr_use_active && mydns_rr_use_stamp && mydns_rr_use_serial) {
      /* We can do incrementals */
      /* Need to send an IXFR if available */
      /*
       * Work out when the client SOA came into being
       */
      MYDNS_RR	*ThisRR = NULL, *rr = NULL;
      char	*deltafilter = NULL;
      int	deletecount, activecount, zonesize;
      size_t	deltasize, fullsize;
       
      /* For very large zones we do not want to load all of the records just to give up */
      sql_build_query(&deltafilter, "serial > %u", q->IR.serial);

      /*
       * Compare counts of changes from full zone data
       * ... assumes records are about the same size
       * approximate zone size by 2 * deleted count === actual number of delta records
       */
      deletecount = mydns_rr_count_deleted_filtered(sql,
							soa->id, DNS_QTYPE_ANY, NULL,
							soa->origin, deltafilter);
      activecount = mydns_rr_count_active_filtered(sql,
						       soa->id, DNS_QTYPE_ANY, NULL,
						       soa->origin, deltafilter);
      zonesize = mydns_rr_count_active(sql,
					   soa->id, DNS_QTYPE_ANY, NULL,
					   soa->origin);
      deltasize = deletecount + activecount + 4;
      fullsize = zonesize + 2;

      if ((deletecount < 0) || (activecount < 0) || (zonesize < 0)) {
	RELEASE(deltafilter);
	dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
	return (TASK_FAILED);
      }
      if (deletecount || activecount) {
	if (deltasize >= fullsize) {
	  /* Send a full zone transfer */
	  /* Current Serial first */
	  rrlist_add(t, ANSWER, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
	  t->sort_level++;
	  if (mydns_rr_load_active(sql, &ThisRR, soa->id, DNS_QTYPE_ANY, NULL, soa->origin) == 0) {
	    for (rr = ThisRR; rr; rr = rr->next) {
	      char *name = mydns_rr_append_origin(MYDNS_RR_NAME(rr), soa->origin);
	      rrlist_add(t, ANSWER, DNS_RRTYPE_RR, (void *)rr, name);
	      if (name != MYDNS_RR_NAME(rr)) RELEASE(name);
	    }
	    t->sort_level++;
	    mydns_rr_free(ThisRR);
	    rrlist_add(t, ANSWER, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
	    t->sort_level++;
	  }
	} else {
	  int latest_serial = soa->serial;

	  rrlist_add(t, ANSWER, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
	  t->sort_level++;
	  soa->serial = q->IR.serial;
	  rrlist_add(t, ANSWER, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
	  t->sort_level++;
	  soa->serial = latest_serial;
	  if (mydns_rr_load_deleted_filtered(sql, &ThisRR, soa->id, DNS_QTYPE_ANY, NULL, soa->origin,
					     deltafilter) == 0) {
	    for (rr = ThisRR; rr; rr = rr->next) {
	      char *name = mydns_rr_append_origin(MYDNS_RR_NAME(rr), soa->origin);
	      rrlist_add(t, ANSWER, DNS_RRTYPE_RR, (void *)rr, name);
	      if (name != MYDNS_RR_NAME(rr)) RELEASE(name);
	    }
	    t->sort_level++;
	    mydns_rr_free(ThisRR);
	  }
	  rrlist_add(t, ANSWER, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
	  t->sort_level++;
	  if (mydns_rr_load_active_filtered(sql, &ThisRR, soa->id, DNS_QTYPE_ANY, NULL, soa->origin,
					    deltafilter) == 0) {
	    for (rr = ThisRR; rr; rr = rr->next) {
	      char *name = mydns_rr_append_origin(MYDNS_RR_NAME(rr), soa->origin);
	      rrlist_add(t, ANSWER, DNS_RRTYPE_RR, (void *)rr, name);
	      if (name != MYDNS_RR_NAME(rr)) RELEASE(name);
	    }
	    t->sort_level++;
	    mydns_rr_free(ThisRR);
	    rrlist_add(t, ANSWER, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
	    t->sort_level++;
	  }
	  RELEASE(deltafilter);
	}
	goto FINISHEDIXFR;
      }
    }
  }

  /* Tell the client to do a full zone transfer or not at all */
  rrlist_add(t, ANSWER, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
  t->sort_level++;

 FINISHEDIXFR:
  mydns_soa_free(soa);

  free_iq(q);

  t->hdr.aa = 1;

  return (TASK_EXECUTED);
}

static taskexec_t
ixfr_purge_all_soas(TASK *t, void *data) {

  /*
   * Retrieve all zone id's that have deleted records.
   *
   * For each zone get the expire field and delete any records that have expired.
   *
   */

  SQL_RES	*res = NULL;
  SQL_ROW	row = NULL;

  size_t	querylen;
  const char	*QUERY0 =	"SELECT DISTINCT zone FROM %s WHERE active='%s'";
  const char	*QUERY1 = 	"SELECT origin FROM %s "
				"WHERE id=%u;";
  const char	*QUERY2 =	"DELETE FROM %s WHERE zone=%u AND active='%s' "
				" AND stamp < DATE_SUB(NOW(),INTERVAL %u SECOND);";
  char		*query = NULL;

  /*
   * Reset task timeout clock to some suitable value in the future
   */
  t->timeout = current_time + ixfr_gc_interval;	/* Try again e.g. tomorrow */

  querylen = sql_build_query(&query, QUERY0,
			     mydns_rr_table_name, mydns_rr_active_types[2]);

  if (!(res = sql_query(sql, query, querylen)))
    ErrSQL(sql, "%s: %s", desctask(t),
	   _("error loading zone id's for DELETED records"));

  RELEASE(query);

  while((row = sql_getrow(res, NULL))) {
    unsigned int	id = atou(row[0]);
    char		*origin = NULL;
    MYDNS_SOA		*soa = NULL;
    SQL_RES		*sres = NULL;

    querylen = sql_build_query(&query, QUERY1,
			       mydns_soa_table_name, id);

    if (!(res = sql_query(sql, query, querylen)))
      ErrSQL(sql, "%s: %s", desctask(t),
	     _("error loading zone from DELETED record zone id"));

    RELEASE(query);

    if (!(row = sql_getrow(res, NULL))) {
      Warnx(_("%s: no soa found for soa id %u"), desctask(t),
	    id);
      continue;
    }

    origin = row[0];

    if (mydns_soa_load(sql, &soa, origin) == 0) {
      querylen = sql_build_query(&query, QUERY2,
				 mydns_rr_table_name, soa->id, mydns_rr_active_types[2], soa->expire);

      if (sql_nrquery(sql, query, querylen) != 0)
	WarnSQL(sql, "%s: %s %s", desctask(t),
		_("error deleting expired records for zone "), soa->origin);

      RELEASE(query);

      sql_free(sres);
    }
  }

  sql_free(res);
  RELEASE(query);     

  return (TASK_CONTINUE);
}

void
ixfr_start() {

  TASK *inittask = NULL;

  if (!ixfr_gc_enabled) return;

  /* Only GC if the DB has IXFR information in it */
  if (mydns_rr_use_active && mydns_rr_use_stamp && mydns_rr_use_serial) {
    inittask = Ticktask_init(LOW_PRIORITY_TASK, NEED_TASK_RUN, -1, 0, AF_UNSPEC, NULL);
    task_add_extension(inittask, NULL, NULL, NULL, ixfr_purge_all_soas);

    inittask->timeout = current_time + ixfr_gc_delay; /* Run first one in e.g. 10 minutes time */
  }
}

/* vi:set ts=3: */
/* NEED_PO */
