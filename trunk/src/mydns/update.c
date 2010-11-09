/**************************************************************************************************
	$Id: update.c,v 1.10 2005/12/18 19:16:41 bboy Exp $
	update.c: Code to implement RFC 2136 (DNS UPDATE)

	Copyright (C) 2005  Don Moore <bboy@bboy.net>

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
#define	DEBUG_UPDATE	1

#define	DEBUG_UPDATE_SQL	1


typedef struct _update_query_rr {
  dns_qtype_t		type;
  dns_class_t		class;
  uint32_t		ttl;
  uchar			*name;
  struct {
    uint16_t		len;
    uchar		*value;
  } rdata;
} UQRR;

#define UQRR_NAME(__rrp)	((__rrp)->name)
#define UQRR_DATA_LENGTH(__rrp)	((__rrp)->rdata.len)
#define UQRR_DATA_VALUE(__rrp)	((__rrp)->rdata.value)
#define UQRR_DATA(__rrp)	((__rrp)->rdata)

/* This is the temporary RRset described in RFC 2136, 3.2.3 */
typedef struct _update_temp_rrset {
  dns_qtype_t		type;
  uint32_t		aux;
  int			checked;		/* Have we checked this unique name/type? */
  uchar			*name;
  struct {
    uint16_t		len;
    uchar		*value;
  } tdata;
  struct {
    uint16_t		len;
    uchar		*value;
  } tedata;
} TMPRR;

#define TMPRR_NAME(__rrp)		((__rrp)->name)
#define TMPRR_DATA_LENGTH(__rrp)	((__rrp)->tdata.len)
#define TMPRR_DATA_VALUE(__rrp)		((__rrp)->tdata.value)
#define TMPRR_DATA(__rrp)		((__rrp)->tdata)
#define TMPRR_EDATA_LENGTH(__rrp)	((__rrp)->tedata.len)
#define TMPRR_EDATA_VALUE(__rrp)	((__rrp)->tedata.value)
#define TMPRR_EDATA(__rrp)		((__rrp)->tedata)

typedef struct _update_query {
  /* Zone section */
  dns_qtype_t		type;			/* Must be DNS_QTYPE_SOA */
  dns_class_t		class;			/* The zone's class */

  UQRR			*PR;			/* Prerequisite section RRs */
  int			numPR;			/* Number of items in 'PR' */

  UQRR			*UP;			/* Update section RRs */
  int			numUP;			/* Number of items in 'UP' */

  UQRR			*AD;			/* Additional data section RRs */
  int			numAD;			/* Number of items in 'AD' */

  TMPRR			**tmprr;		/* Temporary RR list for prerequisite */
  int			num_tmprr;		/* Number of items in "tmprr" */
  uchar			*name;			/* The zone name */
} UQ;

#define UQ_NAME(__qp)		((__qp)->name)

/**************************************************************************************************
	FREE_UQ
	Frees a 'UQ' structure.
**************************************************************************************************/
static void
free_uqrr_data(UQRR *uqrr) {
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("free_uqrr_data freeing %p"), uqrr);
#endif
  UQRR_DATA_LENGTH(uqrr) = 0;
  RELEASE(UQRR_DATA_VALUE(uqrr));
  RELEASE(UQRR_NAME(uqrr));

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("free_uqrr_data freed %p"), uqrr);
#endif
}

static void
free_tmprr(TMPRR *tmprr) {
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("free_tmprr freeing %p"), tmprr);
#endif
  TMPRR_DATA_LENGTH(tmprr) = 0;
  RELEASE(TMPRR_DATA_VALUE(tmprr));
  TMPRR_EDATA_LENGTH(tmprr) = 0;
  TMPRR_EDATA_VALUE(tmprr) = NULL;
  RELEASE(TMPRR_NAME(tmprr));

  RELEASE(tmprr);
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("free_tmprr freed %p"), tmprr);
#endif
}

static void
free_uq(UQ *uq) {
  int n;

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("free_uq freeing %p"), uq);
#endif
  for (n = 0; n < uq->numPR; n++)
    free_uqrr_data(&uq->PR[n]);
  RELEASE(uq->PR);
  for (n = 0; n < uq->numUP; n++)
    free_uqrr_data(&uq->UP[n]);
  RELEASE(uq->UP);
  for (n = 0; n < uq->numAD; n++)
    free_uqrr_data(&uq->AD[n]);
  RELEASE(uq->AD);

  if (uq->num_tmprr) {
    for (n = 0; n < uq->num_tmprr; n++)
      free_tmprr(uq->tmprr[n]);
    RELEASE(uq->tmprr);
  }

  RELEASE(UQ_NAME(uq));

  RELEASE(uq);
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("free_uq freed %p"), uq);
#endif
}
/*--- free_uq() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	UPDATE_TRANSACTION
	Start/commit/rollback a transaction for the UPDATE queries.
	Returns 0 on success, -1 on failure.
**************************************************************************************************/
static int
update_transaction(TASK *t, const char *query) {
  if (sql_nrquery(sql, query, strlen(query)) != 0) {
    WarnSQL(sql, _("%s: Transaction failed to %s"), desctask(t), query);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
  }
  return 0;
}
/*--- update_transaction() ----------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_UPDATE
	If the "update" column exists in the soa table,
	it should contain a list of wildcards separated
	by commas.  In order for the DNS UPDATE to continue, one of the wildcards must match the
	client's IP address.  Returns 0 if okay, -1 if denied.
**************************************************************************************************/
static int
check_update(TASK *t, MYDNS_SOA *soa) {
  SQL_RES	*res = NULL;
  SQL_ROW	row = NULL;
  const char	*ip = NULL;
  char		*query = NULL;
  size_t	querylen = 0;
  int		ok = 0;

  ip = clientaddr(t);

  /* If the 'soa' table does not have an 'update' column, listing access rules, allow
     DNS UPDATE only from 127.0.0.1 and all local addresses */
  if (!mydns_soa_use_update_acl) {
    char **allLocalAddresses = all_interface_addresses();

    if (allLocalAddresses) {
      while(allLocalAddresses[0]) {
	if (!strcmp(ip, allLocalAddresses[0])) {
	  ok = 1;
	  break;
	}
	allLocalAddresses = &allLocalAddresses[1];
      }
    }

    if (ok || !strcmp(ip, "127.0.0.1")) {			/* OK from localhost */
      return 0;
    }
    return dnserror(t, DNS_RCODE_REFUSED, ERR_NO_UPDATE);
  }

  querylen = sql_build_query(&query, "SELECT update_acl FROM %s WHERE id=%u%s%s%s",
			     mydns_soa_table_name, soa->id,
			     (mydns_soa_use_active)? " AND active='" : "",
			     (mydns_soa_use_active)? mydns_soa_active_types[0] : "",
			     (mydns_soa_use_active)? "'" : "");

#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
  DebugX("update-sql", 1, _("%s: DNS UPDATE: %s"), desctask(t), query);
#endif

  res = sql_query(sql, query, querylen);
  RELEASE(query);
  if (!res) {
    ErrSQL(sql, "%s: %s", desctask(t), _("error loading DNS UPDATE access rules"));
  }

  if ((row = sql_getrow(res, NULL))) {
    char *wild, *r;

#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: checking DNS UPDATE access rule '%s'"), desctask(t), row[0]);
#endif
    for (r = row[0]; !ok && (wild = strsep(&r, ",")); ) {
      if (strchr(wild, '/')) {
	if (t->family == AF_INET)
	  ok = in_cidr(wild, t->addr4.sin_addr);
      }	else if (wildcard_match(wild, (char*)ip))
	ok = 1;
    }
  }
  sql_free(res);

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: checked DNS UPDATE access rule '%s' res = %s"), desctask(t), row[0],
	 (ok)?"OK":"FAILED");
#endif
  if (!ok)
    return dnserror(t, DNS_RCODE_REFUSED, ERR_NO_UPDATE);

  return 0;
}
/*--- check_update() ----------------------------------------------------------------------------*/


#if DEBUG_ENABLED && DEBUG_UPDATE
/**************************************************************************************************
	UPDATE_RRDUMP
**************************************************************************************************/
static void
update_rrdump(TASK *t, const char *section, int which, UQRR *rr) {
  char		*buf = NULL;
  char		*b = NULL;
  int		n = 0;
  int		bufused = 0;
  int		bufsiz = BUFSIZ;

  buf = ALLOCATE(bufsiz, char[]);
  b = buf;

  for (n = 0; n < UQRR_DATA_LENGTH(rr); n++) {
    if (isalnum(UQRR_DATA_VALUE(rr)[n])) {
      b += sprintf(b, "%c", UQRR_DATA_VALUE(rr)[n]);
      bufused += 1;
    } else {
      int blen = sprintf(b, "<%d>", UQRR_DATA_VALUE(rr)[n]);
      b += blen;
      bufused += blen;
    }
    if (bufused > (bufsiz - 6)) {
      bufsiz += BUFSIZ;
      buf = REALLOCATE(buf, bufsiz, char[]);
    }
  }

  DebugX("update", 1, _("%s: DNS UPDATE: >>> %s %d: name=[%s] type=%s class=%s ttl=%u rdlength=%u rdata=[%s]"),
	 desctask(t), section, which, UQRR_NAME(rr),
	 mydns_qtype_str(rr->type), mydns_class_str(rr->class),
	 rr->ttl, UQRR_DATA_LENGTH(rr), buf);

  RELEASE(buf);
}
/*--- update_rrdump() ---------------------------------------------------------------------------*/
#endif


/**************************************************************************************************
	UPDATE_GOBBLE_RR
	Reads the next RR from the query.  Returns the new source or NULL on error.
**************************************************************************************************/
static uchar *
update_gobble_rr(TASK *t, uchar *query, size_t querylen, uchar *current, UQRR *rr) {
  uchar		*src = current;
  task_error_t	errcode = 0;
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: update_gobble_rr query=%s, querylen=%u, current=%p, rr=%p"),
	 desctask(t), query, (unsigned int)querylen, current, rr);
#endif

  if (!(UQRR_NAME(rr) = name_unencode2(query, querylen, &src, &errcode))) {
    formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
    return NULL;
  }

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: update_gobble_rr unencoded name src=%s"), desctask(t), UQRR_NAME(rr));
#endif

  DNS_GET16(rr->type, src);
  DNS_GET16(rr->class, src);
  DNS_GET32(rr->ttl, src);
  DNS_GET16(UQRR_DATA_LENGTH(rr), src);
  UQRR_DATA_VALUE(rr) = ALLOCATE(UQRR_DATA_LENGTH(rr)+1, char[]);

  memcpy(UQRR_DATA_VALUE(rr), src, UQRR_DATA_LENGTH(rr));
  src += UQRR_DATA_LENGTH(rr);

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: update_gobble_rr returning %s"), desctask(t), src);
#endif

  return src;
}
/*--- update_gobble_rr() ------------------------------------------------------------------------*/


/**************************************************************************************************
	PARSE_UPDATE_QUERY
	Parses the various sections of the update query.
	Returns 0 on success, -1 on error.
**************************************************************************************************/
static taskexec_t
parse_update_query(TASK *t, UQ *q) {
  uchar		*query = (uchar*)t->query;				/* Start of query section */
  int		querylen = t->len;				/* Length of 'query' */
  uchar		*src = query + DNS_HEADERSIZE;			/* Current position in 'query' */
  int		n = 0;
  task_error_t	errcode = 0;

  /*
  **  Zone section (RFC 2136 2.3)
  */
  if (!(UQ_NAME(q) = name_unencode2(query, querylen, &src, &errcode)))
    return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
  DNS_GET16(q->type, src);
  DNS_GET16(q->class, src);
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: parse_update_query called with q = %p"), desctask(t), q);
  DebugX("update", 1, _("%s:   ZONE: name=[%s]  type=%s  class=%s"), desctask(t),
	 UQ_NAME(q), mydns_qtype_str(q->type), mydns_class_str(q->class));
#endif

  /* ZONE: Must contain exactly one RR with type SOA (RFC 2136 3.1.1) */
  if (t->qdcount != 1)
    return dnserror(t, DNS_RCODE_FORMERR, ERR_MULTI_QUESTIONS);
  if (q->type != DNS_QTYPE_SOA)
    return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_TYPE);

  /*
  **  Prerequisite section (RFC 2136 2.4)
  **  These records are in normal RR format (RFC 1035 4.1.3)
  */
  q->numPR = t->ancount;
  q->PR = ALLOCATE_N(q->numPR, sizeof(UQRR), UQRR[]);
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: parse_update_query checking prerequisites q->numPR = %d, q->PR = %p"),
	 desctask(t), q->numPR, q->PR);
#endif
  for (n = 0; n < q->numPR; n++)
    if (!(src = update_gobble_rr(t, query, querylen, src, &q->PR[n])))
      return -1;
#if DEBUG_ENABLED && DEBUG_UPDATE
  for (n = 0; n < q->numPR; n++)
    update_rrdump(t, "PREREQ", n, &q->PR[n]);
#endif

  /*
  **  Update section (RFC 2136 2.5)
  **  These records are in normal RR format (RFC 1035 4.1.3)
  */
  q->numUP = t->nscount;
  q->UP = ALLOCATE_N(q->numUP, sizeof(UQRR), UQRR[]);
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: parse_update_query doing update section q->numUP = %d, q->UP = %p"),
	 desctask(t), q->numUP, q->UP);
#endif
  for (n = 0; n < q->numUP; n++)
    if (!(src = update_gobble_rr(t, query, querylen, src, &q->UP[n])))
      return -1;
#if DEBUG_ENABLED && DEBUG_UPDATE
  for (n = 0; n < q->numUP; n++)
    update_rrdump(t, "UPDATE", n, &q->UP[n]);
#endif

  /*
  **  Additional data section (RFC 2136 2.6)
  **  These records are in normal RR format (RFC 1035 4.1.3)
  */
  q->numAD = t->arcount;
  q->AD = ALLOCATE_N(q->numAD, sizeof(UQRR), UQRR[]);
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: parse_update_query doing additional data section q->numAD = %d, q->AD = %p"),
	 desctask(t), q->numAD, q->AD);
#endif
  for (n = 0; n < q->numAD; n++)
    if (!(src = update_gobble_rr(t, query, querylen, src, &q->AD[n])))
      return -1;
#if DEBUG_ENABLED && DEBUG_UPDATE
  for (n = 0; n < q->numAD; n++)
    update_rrdump(t, " ADD'L", n, &q->AD[n]);
#endif

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: parse_update_query returning OK"), desctask(t));
#endif
  return (TASK_EXECUTED);
}
/*--- parse_update_query() ----------------------------------------------------------------------*/


/**************************************************************************************************
	TEXT_RETRIEVE
	Retrieve a name from the source without end-dot encoding.
**************************************************************************************************/
static char *
text_retrieve(unsigned char **srcp, unsigned char *end, size_t *datalen, int one_word_only) {
  size_t	n = 0;
  int		x = 0;							/* Offset in 'data' */
  char 		*data = NULL;
  unsigned char	*src = *srcp;

  *datalen = 0;

  for (n = 0; src < end; ) {
    int len = *src++;

    if (n >= *datalen) {
      *datalen += len;
      data = REALLOCATE(data, (*datalen)+1, char[]);
    }
    if (n)
      data[n++] = '\0';
    for (x = 0; x < len && src < end; x++)
      data[n++] = *src++;
    if (one_word_only)
      break;
  }
  data[n] = '\0';
  *srcp = src;
  return data;
}
/*--- text_retrieve() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	UPDATE_GET_RR_DATA
	Sets 'data' and 'aux'.
	Returns 0 on success, -1 on error.
**************************************************************************************************/
static taskexec_t
update_get_rr_data(TASK *t, UQRR *rr, char **data, size_t *datalen,
		   char **edata, size_t *edatalen, uint32_t *aux) {
  unsigned char	*src = (unsigned char*)UQRR_DATA_VALUE(rr);
  unsigned char	*end = (unsigned char*)(UQRR_DATA_VALUE(rr) + UQRR_DATA_LENGTH(rr));
  task_error_t	errcode = 0;

  *edata = NULL;
  *edatalen = 0;

  *aux = 0;

  if (!UQRR_DATA_LENGTH(rr))
    return (TASK_ABANDONED);

  switch (rr->type) {

  case DNS_QTYPE_UNKNOWN:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_NONE:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_A:
    if (UQRR_DATA_LENGTH(rr) != 4)
      return (TASK_ABANDONED);
    *datalen = ASPRINTF(data, "%d.%d.%d.%d", UQRR_DATA_VALUE(rr)[0], UQRR_DATA_VALUE(rr)[1],
			UQRR_DATA_VALUE(rr)[2], UQRR_DATA_VALUE(rr)[3]);
    break;

  case DNS_QTYPE_NS:
    if (!(*data = (char*)name_unencode2((uchar*)t->query, t->len, (uchar**)&src, &errcode)))
      return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
    *datalen = strlen(*data);
    break;

  case DNS_QTYPE_MD:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_MF:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_CNAME:
    if (!(*data = (char*)name_unencode2((uchar*)t->query, t->len, (uchar**)&src, &errcode)))
      return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
    *datalen = strlen(*data);
    break;

  case DNS_QTYPE_SOA:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_MB:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_MG:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_MR:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_NULL:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_WKS:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_PTR:
    if (!(*data = (char*)name_unencode2((uchar*)t->query, t->len, (uchar**)&src, &errcode)))
      return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
    *datalen = strlen(*data);
    break;

  case DNS_QTYPE_HINFO: {
    char	*data1 = NULL;
    char	*data2 = NULL;
    char	*c = NULL;
    size_t	datalen1= 0;
    size_t	datalen2 = 0;
    int	data1sp = 0;
    int	data2sp = 0;

    data1 = text_retrieve(&src, end, &datalen1, 1);
    data2 = text_retrieve(&src, end, &datalen2, 1);

    /* See if either value contains spaces, so we can enclose it with quotes */
    for (c = data1, data1sp = 0; *c && !data1sp; c++)
      if (isspace(*c)) data1sp = 1;
    for (c = data2, data2sp = 0; *c && !data2sp; c++)
      if (isspace(*c)) data2sp = 1;

    *datalen = ASPRINTF(data, "%s%s%s %s%s%s",
			data1sp ? "\"" : "", data1, data1sp ? "\"" : "",
			data2sp ? "\"" : "", data2, data2sp ? "\"" : "");
    RELEASE(data1);
    RELEASE(data2);
  }
    break;

  case DNS_QTYPE_MINFO:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_MX:
    DNS_GET16(*aux, src);
    if (!(*data = (char*)name_unencode2((uchar*)t->query, t->len, (uchar**)&src, &errcode)))
      return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
    *datalen = strlen(*data);
    break;

  case DNS_QTYPE_TXT:
    *data = text_retrieve(&src, end, datalen, 0);
    if (*datalen > DNS_MAXTXTLEN) {
      RELEASE(*data);
      return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_DATA);
    }
    break;

  case DNS_QTYPE_RP: {
    char *data1, *data2;

    if (!(data1 = (char*)name_unencode2((uchar*)t->query, t->len, (uchar**)&src, &errcode)))
      return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
    if (!(data2 = (char*)name_unencode2((uchar*)t->query, t->len, (uchar**)&src, &errcode))) {
      RELEASE(data1);
      return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
    }

    *datalen = ASPRINTF(data, "%s %s", data1, data2);
    RELEASE(data1);
    RELEASE(data2);
  }
    break;

  case DNS_QTYPE_AFSDB:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_X25:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_ISDN:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_RT:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_NSAP:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_NSAP_PTR:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_SIG:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_KEY:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_PX:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_GPOS:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_AAAA:
    if (UQRR_DATA_LENGTH(rr) != 16)
      return (TASK_ABANDONED);
    /* Need to allocate a dynamic buffer */
    if (!(*data = (char*)ipaddr(AF_INET6, UQRR_DATA_VALUE(rr)))) {
      *datalen = 0;
      return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_ADDRESS);
    }
    *datalen = strlen(*data);
    *data = STRDUP(*data);
    break;

  case DNS_QTYPE_LOC:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_NXT:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_EID:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_NIMLOC:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_SRV: {
    uint16_t weight, port;
    char *data1;

    DNS_GET16(*aux, src);
    DNS_GET16(weight, src);
    DNS_GET16(port, src);

    if (!(data1 = (char*)name_unencode2((uchar*)t->query, t->len, (uchar**)&src, &errcode)))
      return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);

    *datalen = ASPRINTF(data, "%u %u %s", weight, port, data1);
    RELEASE(data1);
  }
    break;

  case DNS_QTYPE_ATMA:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_NAPTR:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_KX:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_CERT:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_A6:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_DNAME:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_SINK:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_OPT:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_APL:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_DS:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_SSHFP:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_IPSECKEY:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_RRSIG:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_NSEC:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_DNSKEY:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_DHCID:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_NSEC3:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_NSEC3PARAM:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_HIP:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_SPF:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_UINFO:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_UID:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_GID:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_UNSPEC:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_TKEY:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_TSIG:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_IXFR:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_AXFR:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_MAILB:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_MAILA:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_ANY:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_TA:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

  case DNS_QTYPE_DLV:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);

#if ALIAS_ENABLED
  case DNS_QTYPE_ALIAS:
    *datalen = ASPRINTF(data, "Unknown type %s", mydns_qtype_str(rr->type));
    return (TASK_FAILED);
#endif

  }

  if (mydns_rr_extended_data && *datalen > mydns_rr_data_length) {
    *edatalen = *datalen - mydns_rr_data_length;
    *edata = &((*data)[mydns_rr_data_length]);
    *datalen = mydns_rr_data_length;
  }

  return (TASK_EXECUTED);
}
/*--- update_get_rr_data() ----------------------------------------------------------------------*/


/**************************************************************************************************
	UPDATE_IN_ZONE
	Checks to see if 'name' is within 'origin'.
	Returns 1 if it is, 0 if it's not.
**************************************************************************************************/
static int
update_in_zone(uchar *name, char *origin) {

  if (strlen(origin) > strlen((char*)name))
    return 0;

  if (strcasecmp(origin, (char*)(name + strlen((char*)name) - strlen(origin))))
    return 0;

  return 1;
}
/*--- update_in_zone() --------------------------------------------------------------------------*/

static void
update_escape_name(TASK *t, MYDNS_SOA *soa, UQRR *rr, char **xname, char **xhost) {
  char		*tmp = NULL;

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: update_escape_name"), desctask(t));
#endif

  *xname = sql_escstr(sql, (char*)UQRR_NAME(rr));

  ASPRINTF(&tmp, "%.*s",
	   strlen((char*)UQRR_NAME(rr)) - strlen(soa->origin) - 1,
	   UQRR_NAME(rr));
  *xhost = sql_escstr(sql, tmp);

  RELEASE(tmp);
}

/**************************************************************************************************
	UPDATE_ZONE_HAS_NAME
	Check to see that there is at least one RR in the zone whose name is the same as the
	prerequisite RR.
	Returns 1 if the name exists, 0 if not, -1 on error.
**************************************************************************************************/
static int
update_zone_has_name(TASK *t, MYDNS_SOA *soa, UQRR *rr) {
  SQL_RES	*res = NULL;
  char		*query = NULL;
  size_t	querylen = 0;
  char		*xname = NULL, *xhost = NULL;
  int		found = 0;

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: update_zone_has_name: does [%s] have an RR for [%s]?"), desctask(t),
	 soa->origin, UQRR_NAME(rr));
#endif

  update_escape_name(t, soa, rr, &xname, &xhost);

  querylen = sql_build_query(&query,
			     "SELECT id FROM %s WHERE zone=%u AND (name='%s' OR name='%s') LIMIT 1",
			     mydns_rr_table_name, soa->id, xhost, xname);
#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
  DebugX("update-sql", 1, _("%s: DNS UPDATE: %s"), desctask(t), query);
#endif

  RELEASE(xname);
  RELEASE(xhost);

  res = sql_query(sql, query, querylen);
  RELEASE(query);
  if (!(res))	{
    WarnSQL(sql, "%s: %s", desctask(t), _("error searching name for DNS UPDATE"));
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
  }

  if (sql_num_rows(res) > 0)
    found = 1;

  sql_free(res);

  return (found);
}
/*--- update_zone_has_name() --------------------------------------------------------------------*/


/**************************************************************************************************
	UPDATE_ZONE_HAS_RRSET
	Check to see that there is an RRset in the zone whose name and type are the same as the
	prerequisite RR.
	Returns 1 if the name exists, 0 if not, -1 on error.
**************************************************************************************************/
static int
update_zone_has_rrset(TASK *t, MYDNS_SOA *soa, UQRR *rr) {
  SQL_RES	*res = NULL;
  char		*query = NULL;
  size_t	querylen = 0;
  char		*xname = NULL, *xhost = NULL;
  int		found = 0;

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: update_zone_has_rrset: does [%s] have an RR for [%s] with type %s?"),
	 desctask(t),
	 soa->origin, UQRR_NAME(rr), mydns_qtype_str(rr->type));
#endif

  update_escape_name(t, soa, rr, &xname, &xhost);

  querylen = sql_build_query(&query,
			     "SELECT id FROM %s "
			     " WHERE zone=%u AND (name='%s' OR name='%s') AND type='%s' LIMIT 1",
			     mydns_rr_table_name, soa->id, xhost, xname, mydns_qtype_str(rr->type));
#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
  DebugX("update-sql", 1, _("%s: DNS UPDATE: %s"), desctask(t), query);
#endif

  RELEASE(xname);
  RELEASE(xhost);

  res = sql_query(sql, query, querylen);
  RELEASE(query);
  if (!(res)) {
    WarnSQL(sql, "%s: %s", desctask(t), _("error searching name/type for DNS UPDATE"));
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
  }

  if (sql_num_rows(res) > 0)
    found = 1;

  sql_free(res);

  return (found);
}
/*--- update_zone_has_rrset() -------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_PREREQUISITE
	Check the specified prerequisite as described in RFC 2136 3.2.
	Returns 0 on success, -1 on error.
**************************************************************************************************/
static taskexec_t
check_prerequisite(TASK *t, MYDNS_SOA *soa, UQ *q, UQRR *rr) {
  char		*data = NULL, *edata = NULL;
  size_t	datalen = 0, edatalen = 0;
  uint32_t	aux = 0;
  int		n = 0, rv = 0;

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite: rr->name=[%s]"),
	 desctask(t), UQRR_NAME(rr));
  DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite: rr->class=%s"),
	 desctask(t), mydns_class_str(rr->class));
  DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite: q->class=%s"),
	 desctask(t), mydns_class_str(q->class));
  DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite: rr->type=%s"),
	 desctask(t), mydns_qtype_str(rr->type));
  DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite: rr->rdlength=%u"),
	 desctask(t), UQRR_DATA_LENGTH(rr));
#endif

  /* Get aux/data */
  update_get_rr_data(t, rr, &data, &datalen, &edata, &edatalen, &aux);	/* Ignore error */

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite: aux=%u"), desctask(t), aux);
  DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite: data=[%s]"), desctask(t), (data)?data:"NULL");
#endif

  RELEASE(data);

  /* TTL must be zero */
  if (rr->ttl) {
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite failed: TTL nonzero"), desctask(t));
#endif
    return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_TTL);	
  }

  /* rr->name must be in-zone */
  if (!update_in_zone(UQRR_NAME(rr), soa->origin)) {
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite failed: name (%s) not in zone (%s)"), desctask(t),
	   UQRR_NAME(rr), soa->origin);
#endif
    return dnserror(t, DNS_RCODE_NOTZONE, ERR_INVALID_DATA);
  }

  /* Following pseudocode from section 3.2.5... */
  if (rr->class == DNS_CLASS_ANY) {
    if (UQRR_DATA_LENGTH(rr)) {
#if DEBUG_ENABLED && DEBUG_UPDATE
      DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite failed: class is ANY but rdlength is nonzero"),
	     desctask(t));
#endif
      return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_DATA);	
    }
    if (rr->type == DNS_QTYPE_ANY) {
      if ((rv = update_zone_has_name(t, soa, rr)) != 1) {
	if (!rv) {
#if DEBUG_ENABLED && DEBUG_UPDATE
	  DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite failed: zone contains no names matching [%s]"),
		 desctask(t), UQRR_NAME(rr));
#endif
	  return dnserror(t, DNS_RCODE_NXDOMAIN, ERR_PREREQUISITE_FAILED);
	} else {
	  return (TASK_FAILED);
	}
      }
    } else if ((rv = update_zone_has_rrset(t, soa, rr)) != 1) {
      if (!rv) {
#if DEBUG_ENABLED && DEBUG_UPDATE
	DebugX("update", 1,
	       _("%s: DNS UPDATE: check_prerequisite failed: zone contains no names matching [%s] with type %s"),
	       desctask(t), UQRR_NAME(rr), mydns_qtype_str(rr->type));
#endif
	return dnserror(t, DNS_RCODE_NXRRSET, ERR_PREREQUISITE_FAILED);
      } else {
	return (TASK_FAILED);
      }
    }
  } else if (rr->class == DNS_CLASS_NONE) {
    if (UQRR_DATA_LENGTH(rr) != 0) {
#if DEBUG_ENABLED && DEBUG_UPDATE
      DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite failed: class is NONE but rdlength is zero"),
	     desctask(t));
#endif
      return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_DATA);	
    }
    if (rr->type == DNS_QTYPE_ANY) {
      if ((rv = update_zone_has_name(t, soa, rr)) != 0) {
	if (rv == 1) {
#if DEBUG_ENABLED && DEBUG_UPDATE
	  DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite failed: zone contains a name matching [%s]"),
		 desctask(t), UQRR_NAME(rr));
#endif
	  return dnserror(t, DNS_RCODE_YXDOMAIN, ERR_PREREQUISITE_FAILED);
	} else {
	  return (TASK_FAILED);
	}
      }
    } else if ((rv = update_zone_has_rrset(t, soa, rr)) != 0) {
      if (rv == 1) {
#if DEBUG_ENABLED && DEBUG_UPDATE
	DebugX("update", 1, _("%s: DNS UPDATE: check_prerequisite failed: zone contains a name matching [%s] with type %s"),
	       desctask(t), UQRR_NAME(rr), mydns_qtype_str(rr->type));
#endif
	return dnserror(t, DNS_RCODE_YXRRSET, ERR_PREREQUISITE_FAILED);
      } else {
	return (TASK_FAILED);
      }
    }
  } else if (rr->class == q->class) {
    int		unique = 0;			/* Is this rrset element unique? */
    taskexec_t	ures = TASK_FAILED;

#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: want to add %s/%s to tmprr"), desctask(t),
	   UQRR_NAME(rr), mydns_qtype_str(rr->type));
#endif

    /* Get the RR data */
    if ((ures = update_get_rr_data(t, rr,
				   &data, &datalen, &edata, &edatalen, &aux)) != TASK_EXECUTED) {
      if (ures != TASK_FAILED)
	return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_DATA);
      else
	return (ures);
    }

#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: for tmprr, data=[%s], aux=%u"), desctask(t), data, aux);
#endif

    /* Add this name/type to the "tmprr" list (in the UQRR struct) */
    /* First, check to make sure it's unique */
    for (n = 0, unique = 1; n < q->num_tmprr && unique; n++)
      if (q->tmprr[n]->type == rr->type && !strcasecmp((char*)TMPRR_NAME(q->tmprr[n]), (char*)UQRR_NAME(rr))
	  && (TMPRR_DATA_LENGTH(q->tmprr[n]) == datalen)
	  && !memcmp(TMPRR_DATA_VALUE(q->tmprr[n]), data, datalen)
	  && q->tmprr[n]->aux == aux)
	unique = 0;

    if (unique) {
      if (!q->num_tmprr)
	q->tmprr = ALLOCATE(sizeof(TMPRR *), TMPRR*[]);
      else
	q->tmprr = REALLOCATE(q->tmprr, sizeof(TMPRR *) * (q->num_tmprr + 1), TMPRR*[]);

      /* Add this stuff to the new tmprr */
      q->tmprr[q->num_tmprr] = ALLOCATE(sizeof(TMPRR), TMPRR);
      strncpy((char*)TMPRR_NAME(q->tmprr[q->num_tmprr]), (char*)UQRR_NAME(rr),
	      sizeof(TMPRR_NAME(q->tmprr[q->num_tmprr])) - 1);
      q->tmprr[q->num_tmprr]->type = rr->type;
      TMPRR_DATA_LENGTH(q->tmprr[q->num_tmprr]) = datalen;
      TMPRR_DATA_VALUE(q->tmprr[q->num_tmprr]) = (uchar*)data;
      TMPRR_EDATA_LENGTH(q->tmprr[q->num_tmprr]) = edatalen;
      TMPRR_EDATA_VALUE(q->tmprr[q->num_tmprr]) = (uchar*)edata;
      q->tmprr[q->num_tmprr]->aux = aux;
      q->tmprr[q->num_tmprr]->checked = 0;
      q->num_tmprr++;
    } else {
      RELEASE(data);
    }
  } else {
    return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_DATA);
  }

  return TASK_EXECUTED;
}
/*--- check_prerequisite() ----------------------------------------------------------------------*/


/**************************************************************************************************
	UPDATE_RRTYPE_OK
	Is this RR type okay with MyDNS?  RFC 2136 3.4.1.2 specifically prohibigs ANY, AXFR, MAILA,
	MAILB, or "any other QUERY metatype" or "any unrecognized type".
	Returns 1 if OK, 0 if not OK.
**************************************************************************************************/
static inline int
update_rrtype_ok(dns_qtype_t type) {
  switch (type) {
    /* We support UPDATEs of these types: */
  case DNS_QTYPE_A:
  case DNS_QTYPE_AAAA:
  case DNS_QTYPE_CNAME:
  case DNS_QTYPE_HINFO:
  case DNS_QTYPE_MX:
  case DNS_QTYPE_NS:
  case DNS_QTYPE_TXT:
  case DNS_QTYPE_PTR:
  case DNS_QTYPE_RP:
  case DNS_QTYPE_SRV:
    return 1;

  default:
    return 0;
  }
  return 0;
}
/*--- update_rrtype_ok() ------------------------------------------------------------------------*/


/**************************************************************************************************
	PRESCAN_UPDATE
	Prescan the specified update section record (RFC 2136 3.4.1).
	Returns 0 on success, -1 on error.
**************************************************************************************************/
static taskexec_t
prescan_update(TASK *t, UQ *q, UQRR *rr) {
  /* Class must be ANY, NONE, or the same as the zone's class */
  if ((rr->class != DNS_CLASS_ANY) && (rr->class != DNS_CLASS_NONE) && (rr->class != q->class)) {
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: prescan_update failed test 1 (check class)"), desctask(t));
#endif
    return dnserror(t, DNS_RCODE_FORMERR, ERR_DB_ERROR);
  }

  /* "Using the definitions in Section 1.2, each RR's NAME must be in the zone specified by the
     Zone Section, else signal NOTZONE to the requestor. */
  /* XXX WTF? */

  /* "For RRs whose CLASS is not ANY, check the TYPE and if it is ANY, AXFR, MAILA, MAILB, or
     any other QUERY metatype, or any unrecognized type, then signal FORMERR to the requestor. */
  if ((rr->class != DNS_CLASS_ANY) && !update_rrtype_ok(rr->type)) {
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: prescan_update failed test 2 (check RR types)"), desctask(t));
#endif
    return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_TYPE);
  }

  /* "For RRs whose CLASS is ANY or NONE, check the TTL to see that it is zero (0), else signal
     a FORMERR to the requestor." */
  if (((rr->class == DNS_CLASS_ANY) || (rr->class == DNS_CLASS_NONE)) && (rr->ttl != 0)) {
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: prescan_update failed test 3 (check TTL)"), desctask(t));
#endif
    return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_TTL);
  }

  /* "For any RR whose CLASS is ANY, check the RDLENGTH to make sure that it is zero (0) (that
     is, the RDATA field is empty), and that the TYPE is not AXFR, MAILA, MAILB, or any other
     QUERY metatype besides ANY, or any unrecognized type, else signal FORMERR to the
     requestor." */
  if ((rr->class == DNS_CLASS_ANY) && (UQRR_DATA_LENGTH(rr) != 0)) {
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: prescan_update failed test 4 (check RDLENGTH)"), desctask(t));
#endif
    return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_DATA);
  }
  if ((rr->class == DNS_CLASS_ANY) && (!update_rrtype_ok(rr->type) && rr->type != DNS_QTYPE_ANY)) {
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: prescan_update failed test 5 (rr->type is %s)"), desctask(t),
	   mydns_qtype_str(rr->type));
#endif
    return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_TYPE);
  }

  return (TASK_EXECUTED);
}
/*--- prescan_update() --------------------------------------------------------------------------*/


/**************************************************************************************************
	UPDATE_ADD_RR
	Add an RR to the zone.
	Returns 0 on success, -1 on failure.
**************************************************************************************************/

/*************************************************************************************************
 *	With the advent of the deleted state for records in the database
 *	 (a new state for 'active') we need to cope with resurrecting old records
 *	we do this by detecting 'duplicates' which postgres already did and replacing them with a
 *	current verision.
 ************************************************************************************************/
static taskexec_t
update_add_rr(TASK *t, MYDNS_SOA *soa, UQRR *rr, uint32_t next_serial) {
  char		*data = NULL, *edata = NULL;
  size_t	datalen = 0, edatalen = 0;
  uint32_t	aux = 0;
  char		*xhost = NULL, *xname = NULL, *xdata = NULL, *xedata = NULL;
  char		*query = NULL;
  size_t	querylen = 0;
#if USE_PGSQL
  int		duplicate = 0;
#endif
  taskexec_t	ures = TASK_FAILED;

  if ((ures = update_get_rr_data(t, rr,
				 &data, &datalen, &edata, &edatalen, &aux)) != TASK_EXECUTED) {
    if (ures != TASK_FAILED)
      return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_DATA);
    else
      return (ures);
  }

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: UPDATE_ADD_RR: %s %u %s %s %u %s"), desctask(t),
	 UQRR_NAME(rr), rr->ttl, mydns_class_str(rr->class), mydns_qtype_str(rr->type), aux, data);
#endif

  /* Construct query */
  update_escape_name(t, soa, rr, &xname, &xhost);

  xdata = sql_escstr2(sql, data, datalen);
  if (edatalen)
    xedata = sql_escstr2(sql, edata, edatalen);

  /* IXFR support code */
  if(mydns_rr_use_active && mydns_rr_use_stamp && mydns_rr_use_serial) {
    SQL_RES	*res = NULL;
    /*
    **  Find out if the record exists - update active records otherwise insert a new active one
    ** We check edatakey as this is indexed
    */
    querylen = sql_build_query(&query,
			       "SELECT id,serial FROM %s "
			       "WHERE zone=%u AND (name='%s' OR name='%s') AND type='%s' "
			       "AND data='%s' AND active='%s'%s%s%s",
			       mydns_rr_table_name, soa->id,
			       xhost, xname, mydns_qtype_str(rr->type), xdata,
			       mydns_rr_active_types[0],
			       (edatalen)?" AND edatakey=md5('":"",
			       (edatalen)?xedata:"",
			       (edatalen)?"')":"");

    res = sql_query(sql, query, querylen);
    RELEASE(query);
    if(!(res)) {
      WarnSQL(sql, "%s: %s", desctask(t), _("error searching pre-existing for DNS UPDATE"));
      return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
    }
    if (sql_num_rows(res) > 0) {
      /* Pre-existing record ignore. */
      sql_free(res);
      return (TASK_EXECUTED);
    }
    sql_free(res);
    /* Record needs inserting */
    querylen = sql_build_query(&query,
			       "INSERT INTO %s (zone,name,type,data,aux,ttl,serial,active%s)"
			       " VALUES (%u,'%s','%s','%s',%u,%u,%u,'%s'%s%s%s%s%s%s)",
			       mydns_rr_table_name,
			       (edatalen)?",edata,edatakey":"",
			       soa->id, xhost, mydns_qtype_str(rr->type),
			       xdata, aux, rr->ttl, next_serial, mydns_rr_active_types[0],
			       (edatalen)?",'":"",
			       (edatalen)?xedata:"",
			       (edatalen)?"'":"",
			       (edatalen)?",md5('":"",
			       (edatalen)?xedata:"",
			       (edatalen)?"')":"");
    if (sql_nrquery(sql, query, querylen) != 0) {
      WarnSQL(sql, "%s: %s %s", desctask(t), _("error updating entries using"), query);
      RELEASE(query);
      return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
    }
  } else {
    /* Non IXFR Support Code */
    /* POSTGRES ONLY */
    /* First we have to see if this record exists.  If it does, we should "silently ignore" it. */
#if USE_PGSQL
    SQL_RES	*res = NULL;

    /*
    ** This is only necessary for Postgres, we can use "INSERT IGNORE" with MySQL
    */
    querylen = sql_build_query(&query,
			       "SELECT id FROM %s "
			       "WHERE zone=%u AND (name='%s' OR name='%s') AND type='%s' "
			       "AND data='%s'%s%s%s LIMIT 1",
			       mydns_rr_table_name, soa->id,
			       xhost, xname, mydns_qtype_str(rr->type), xdata,
			       (edatalen)?" AND edatakey=md5('":"",
			       (edatalen)?xedata:"",
			       (edatalen)?"')":"");
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: UPDATE_ADD_RR: %s"), desctask(t), query);
#else
#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
    DebugX("update", 1, _("%s: DNS UPDATE: %s"), desctask(t), query);
#endif
#endif
    res = sql_query(sql, query, querylen);
    RELEASE(query);
    if (!res) {
      WarnSQL(sql, "%s: %s", desctask(t), _("error searching duplicate for DNS UPDATE"));
      return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
    }
    if (sql_num_rows(res) > 0)
      duplicate = 1;
    sql_free(res);
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: UPDATE_ADD_RR: duplicate=%d"), desctask(t), duplicate);
#endif
#endif
    /* END POSTGRES ONLY */

    /*
     * For Postgresql we need to update any deleted but not
     * inactive records to be active with the latest serial number
     */
#if USE_PGSQL
    if (!duplicate)
#endif
      {
	char *serialstr = NULL;
	if (mydns_rr_use_serial)
	  ASPRINTF(&serialstr, "%u", next_serial);
	querylen = sql_build_query(&query,
#if USE_PGSQL
				   "INSERT INTO %s"
#else
				   "INSERT IGNORE INTO %s"
#endif
				   " (zone,name,type,data,aux,ttl"
				   /* Optional edata */
				   "%s"
				   /* Optional Active key */
				   "%s"
				   /* Optional serial number */
				   "%s"
				   ")"
				   " VALUES (%u,'%s','%s','%s',%u,%u"
				   /* Optional edata values */
				   "%s%s%s%s%s%s"
				   /* Optional Active values */
				   "%s%s%s"
				   /* Optional Serial number */
				   "%s"
				   ")",
				   mydns_rr_table_name,
				   (edatalen) ?",edata,edatakey" : "",
				   mydns_rr_use_active ? ",active" : "",
				   mydns_rr_use_serial ? ",serial" : "",
				   soa->id, xhost, mydns_qtype_str(rr->type), xdata, aux, rr->ttl,
				   (edatalen) ? ",'" : "",
				   (edatalen) ? xedata : "",
				   (edatalen) ? "'" : "",
				   (edatalen) ? ",md5('" : "",
				   (edatalen) ? xedata : "",
				   (edatalen) ? "')" : "",
				   (mydns_rr_use_active)? ",'" : "",
				   (mydns_rr_use_active)? mydns_rr_active_types[0] : "",
				   (mydns_rr_use_active)? "'" : "",
				   (mydns_rr_use_serial)? serialstr : ""
				   );
	RELEASE(serialstr);
  /* Execute the query */
#if DEBUG_ENABLED && DEBUG_UPDATE
	DebugX("update", 1, _("%s: DNS UPDATE: ADD RR: %s"), desctask(t), query);
#else
#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
	DebugX("update", 1, _("%s: DNS UPDATE: %s"), desctask(t), query);
#endif
#endif

	if (sql_nrquery(sql, query, querylen) != 0) {
	  WarnSQL(sql, "%s: %s", desctask(t), _("error adding RR via DNS UPDATE"));
	  RELEASE(query);
	  return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
	}
      }
  }

  RELEASE(xhost);
  RELEASE(xname);
  RELEASE(xdata);
  RELEASE(xedata);
  RELEASE(query);

  /* Output info to verbose log */
  { char	*tmp = NULL;
    ASPRINTF(&tmp, "ADD %s %u IN %s %u %s",
	     UQRR_NAME(rr), rr->ttl, mydns_qtype_str(rr->type), aux, data);
    task_output_info(t, tmp);
    RELEASE(tmp);
  }
  t->update_done++;
  RELEASE(data);

  return (TASK_EXECUTED);
}
/*--- update_add_rr() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	UPDATE_DELETE_RRSET_ALL
	Deletes all RRsets from the zone for a specified name.
	Returns 0 on success, -1 on failure.
**************************************************************************************************/
static taskexec_t
update_delete_rrset_all(TASK *t, MYDNS_SOA *soa, UQRR *rr, uint32_t next_serial) {
  char		*xname = NULL, *xhost = NULL;
  char		*query = NULL;
  size_t	querylen = 0;
  SQL_RES	*res = NULL;
  int		res2 = 0;
  int		updates = 0;

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: UPDATE_DELETE_RRSET_ALL: %s %u %s %s"), desctask(t),
	 UQRR_NAME(rr), rr->ttl, mydns_class_str(rr->class), mydns_qtype_str(rr->type));
#endif

  /* Delete rrset - check both the FQDN and the hostname without trailing dot */
  update_escape_name(t, soa, rr, &xname, &xhost);

  if (mydns_rr_use_active && mydns_rr_use_stamp && mydns_rr_use_serial) {
    SQL_ROW	row = NULL;
    unsigned long *lengths = NULL;

    /*
     * This is complicated as we want to purge records that match current active ones
     * we are going to make deleted before we mark them deleted.
     * (Otherwise we will get a key clash in the database)
     */
    /*
     * Phase 1 get all active records in the current RR set and delete any matching deleted
     * records in the database.
     *
     * We can't do this with an inner select as this is an operation on the same table?
     */
    querylen = sql_build_query(&query,
			       "SELECT name,type,data,aux,ttl%s FROM %s "
			       "WHERE zone=%u AND (name='%s' OR name='%s') AND active='%s'",
			       ((mydns_rr_extended_data)?",edatakey":""),
			       mydns_rr_table_name, soa->id, xname, xhost,
			       mydns_rr_active_types[0]);
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: DELETE RR: %s"), desctask(t), query);
#else
#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
    DebugX("update", 1, _("%s: DNS UPDATE: %s"), desctask(t), query);
#endif
#endif
    res = sql_query(sql, query, querylen);
    RELEASE(query);
    if (!(res)) {
      WarnSQL(sql, "%s: %s", desctask(t), _("error deleting all RRsets via DNS UPDATE"));
      return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
    }
    while ((row = sql_getrow(res, &lengths))) {
      char *xdata = NULL, *xedatakey = NULL;
      xdata = sql_escstr2(sql, row[2], lengths[2]);
      if (mydns_rr_extended_data && row[5] && lengths[5]) {
	xedatakey = sql_escstr2(sql, row[5], lengths[5]);
      }
      querylen = sql_build_query(&query,
				 "DELETE FROM %s "
				 "WHERE zone=%u AND name='%s' AND type='%s' "
				 "AND data='%s' AND aux=%u AND ttl=%u "
				 "AND active='%s'%s%s%s",
				 mydns_rr_table_name,
				 soa->id, row[0], row[1],
				 xdata, atou(row[3]), atou(row[4]),
				 mydns_rr_active_types[2],
				 ((xedatakey) ? " AND edatakey='" : ""),
				 ((xedatakey) ? xedatakey : ""),
				 ((xedatakey) ? "'" : ""));
      RELEASE(xdata);
      RELEASE(xedatakey);
      res2 = sql_nrquery(sql, query, querylen);
      RELEASE(query);
      if (res2 != 0) {
	WarnSQL(sql, "%s: %s - %s",
		desctask(t), _("error purging DELETE RR via DNS UPDATE"), query);
	return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
      }
      updates++;
    }
    sql_free(res);
    /*
     * Phase 2 update all active records in the current RR set to be deleted
     */
    querylen = sql_build_query(&query,
			       "UPDATE %s SET active='%s',serial=%u "
			       "WHERE zone=%u, AND (name='%s' OR name='%s')",
			       mydns_rr_table_name, mydns_rr_active_types[2], next_serial,
			       soa->id, xname, xhost);
  } else {
    updates++;
    querylen = sql_build_query(&query,
			       "DELETE FROM %s WHERE zone=%u AND (name='%s' OR name='%s')",
			       mydns_rr_table_name, soa->id, xname, xhost);
  }
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: DELETE RRSET_ALL: %s"), desctask(t), query);
#else
#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
  DebugX("update", 1, _("%s: DNS UPDATE: %s"), desctask(t), query);
#endif
#endif
  RELEASE(xname);
  RELEASE(xhost);

  /* Execute the query */
  if (updates && sql_nrquery(sql, query, querylen) != 0) {
    WarnSQL(sql, "%s: %s", desctask(t), _("error deleting all RRsets via DNS UPDATE"));
    RELEASE(query);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
  }
  RELEASE(query);

  /* Output info to verbose log */
  ASPRINTF(&query, "DELETE_ALL_RRSET %s", UQRR_NAME(rr));
  task_output_info(t, query);
  RELEASE(query);
  if (updates) t->update_done++;

  return (TASK_EXECUTED);
}
/*--- update_delete_rrset_all() -----------------------------------------------------------------*/


/**************************************************************************************************
	UPDATE_DELETE_RR
	Deletes an RR from the zone for a specified name.
	Returns 0 on success, -1 on failure.
**************************************************************************************************/
static taskexec_t
update_delete_rr(TASK *t, MYDNS_SOA *soa, UQRR *rr, uint32_t next_serial) {
  char		*data = NULL, *edata = NULL;
  size_t	datalen = 0, edatalen = 0;
  uint32_t	aux = 0;
  char		*xname = NULL, *xhost = NULL, *xdata = NULL, *xedata = NULL;
  char		*query = NULL;
  size_t	querylen = 0;
  SQL_RES	*res = NULL;
  int		updates = 0;
  taskexec_t	ures = TASK_FAILED;

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: UPDATE_DELETE_RR: %s %u %s %s"), desctask(t),
	 UQRR_NAME(rr), rr->ttl, mydns_class_str(rr->class), mydns_qtype_str(rr->type));
#endif

  if ((ures = update_get_rr_data(t, rr,
				 &data, &datalen, &edata, &edatalen, &aux)) != TASK_EXECUTED) {
    if (ures != TASK_FAILED)
      return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_DATA);
    else
      return (ures);
  }

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: DELETE RR: %s IN %s %s"), desctask(t),
	 UQRR_NAME(rr), mydns_qtype_str(rr->type), data);
#endif

  update_escape_name(t, soa, rr, &xname, &xhost);

  xdata = sql_escstr2(sql, data, datalen);

  if(edatalen)
    xedata = sql_escstr2(sql, edata, edatalen);

  if (mydns_rr_use_active && mydns_rr_use_stamp && mydns_rr_use_serial) {
    SQL_ROW	row;
    /*
     * This is less complicated as we only want to delete records that match the active one
     */
    /*
     * Phase 1 delete all deleted records in the rr table that match the active one
     *
     * Ignore the ttl in this match - use aux should be part of key?
     *
     * We call SELECT in case we get a retransmission from the update client
     */
    querylen = sql_build_query(&query,
			       "SELECT name,type,data,aux,ttl FROM %s "
			       "WHERE zone=%u AND (name='%s' OR name='%s') AND type='%s' "
			       "AND data='%s' AND aux=%u AND active='%s'%s%s%s",
			       mydns_rr_table_name,
			       soa->id, xname, xhost, mydns_qtype_str(rr->type),
			       xdata, aux, mydns_rr_active_types[0],
			       (edatalen)?" AND edatakey=md5('":"",
			       (edatalen)?xedata:"",
			       (edatalen)?"')":"");
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: DELETE RR: %s"), desctask(t), query);
#else
#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
    DebugX("update", 1, _("%s: DNS UPDATE: %s"), desctask(t), query);
#endif
#endif
    res = sql_query(sql, query, querylen);
    RELEASE(query);
    if (!(res)) {
      RELEASE(xname);
      RELEASE(xhost);
      RELEASE(xdata);
      RELEASE(xedata);
      WarnSQL(sql, "%s: %s", desctask(t), _("error deleting  RR via DNS UPDATE"));
      return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
    }
    while ((row = sql_getrow(res, NULL))) {
      int res2 = 0;
      querylen = sql_build_query(&query,
				 "DELETE FROM %s "
				 "WHERE zone=%u AND (name='%s' OR name='%s') AND type='%s' "
				 "AND data='%s' AND aux=%u AND active='%s'%s%s%s",
				 mydns_rr_table_name,
				 soa->id, xname, xhost, mydns_qtype_str(rr->type),
				 xdata, aux, mydns_rr_active_types[2],
				 (edatalen)?" AND edatakey=md5('":"",
				 (edatalen)?xedata:"",
				 (edatalen)?"')":"");
#if DEBUG_ENABLED && DEBUG_UPDATE
      DebugX("update", 1, _("%s: DNS UPDATE: DELETE RR: %s"), desctask(t), query);
#else
#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
      DebugX("update", 1, _("%s: DNS UPDATE: %s"), desctask(t), query);
#endif
#endif
      res2 = sql_nrquery(sql, query, querylen);
      RELEASE(query);
      if (res2 != 0) {
	RELEASE(xname);
	RELEASE(xhost);
	RELEASE(xdata);
	RELEASE(xedata);
	WarnSQL(sql, "%s: %s", desctask(t), _("error deleting RR via DNS UPDATE"));
	return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
      }
      updates++;
    }
    sql_free(res);
    /*
     * Phase 2 update the active records to be deleted
     */
    querylen = sql_build_query(&query,
			       "UPDATE %s SET active='%s',serial=%u "
			       "WHERE zone=%u AND (name='%s' OR name='%s') AND type='%s' "
			       "AND data='%s' AND aux=%u AND active='%s'%s%s%s",
			       mydns_rr_table_name, mydns_rr_active_types[2], next_serial,
			       soa->id, xname, xhost, mydns_qtype_str(rr->type), xdata, aux,
			       mydns_rr_active_types[0],
			       (edatalen)?" AND edatakey=md5('":"",
			       (edatalen)?xedata:"",
			       (edatalen)?"')":"");
  } else {
    updates++;
    querylen = sql_build_query(&query,
			       "DELETE FROM %s "
			       "WHERE zone=%u AND (name='%s' OR name='%s') AND type='%s' "
			       "AND data='%s' AND aux=%u%s%s%s",
			       mydns_rr_table_name,
			       soa->id, xname, xhost, mydns_qtype_str(rr->type), xdata, aux,
			       (edatalen)?" AND edatakey=md5('":"",
			       (edatalen)?xedata:"",
			       (edatalen)?"')":"");
  }
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: DELETE RR: %s"), desctask(t), query);
#else
#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
  DebugX("update", 1, _("%s: DNS UPDATE: %s"), desctask(t), query);
#endif
#endif
  RELEASE(xname);
  RELEASE(xhost);
  RELEASE(xdata);
  RELEASE(xedata);

  /* Execute the query */
  if (updates && sql_nrquery(sql, query, querylen) != 0) {
    WarnSQL(sql, "%s: %s", desctask(t), _("error deleting RR via DNS UPDATE"));
    RELEASE(query);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
  }
  RELEASE(query);

  /* Output info to verbose log */
  { char	*tmp = NULL;
    ASPRINTF(&tmp, "DELETE %s IN %s %s", UQRR_NAME(rr), mydns_qtype_str(rr->type), data);
    task_output_info(t, tmp);
    RELEASE(tmp);
  }
  if (updates) t->update_done++;
  RELEASE(data);

  return (TASK_EXECUTED);
}
/*--- update_delete_rr() ------------------------------------------------------------------------*/


/**************************************************************************************************
	UPDATE_DELETE_RRSET
	Deletes an RRset from the zone for a specified name.
	Returns 0 on success, -1 on failure.
**************************************************************************************************/
static taskexec_t
update_delete_rrset(TASK *t, MYDNS_SOA *soa, UQRR *rr, uint32_t next_serial) {
  char		*xname = NULL, *xhost = NULL;
  char		*query = NULL;
  size_t	querylen = 0;
  SQL_RES	*res = NULL;
  int		updates = 0;

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: UPDATE_DELETE_RRSET: %s %u %s %s"), desctask(t),
	 UQRR_NAME(rr), rr->ttl, mydns_class_str(rr->class), mydns_qtype_str(rr->type));
#endif

  /* Delete rr - check both the FQDN and the hostname without trailing dot */
  update_escape_name(t, soa, rr, &xname, &xhost);

  if (mydns_rr_use_active && mydns_rr_use_stamp && mydns_rr_use_serial) {
    SQL_ROW	row = NULL;
    unsigned long *lengths = NULL;
    /*
     * This is complicated as we want to purge records that match current active ones
     * we are going to make deleted before we mark them deleted.
     * (Otherwise we will get a key clash in the database)
     */
    /*
     * Phase 1 get all active records in the current RR set and delete any matching deleted
     * records in the database.
     *
     * We can't do this with an inner select as this is an operation on the same table?
     */
    querylen = sql_build_query(&query,
			       "SELECT name,type,data,aux,ttl%s FROM %s "
			       "WHERE zone=%u AND (name='%s' OR name='%s') AND type='%s' "
			       "AND active='%s'",
			       (mydns_rr_extended_data)?",edatakey":"",
			       mydns_rr_table_name,
			       soa->id, xname, xhost, mydns_qtype_str(rr->type),
			       mydns_rr_active_types[0]);
    res = sql_query(sql, query, querylen);
    RELEASE(query);
    if (!res) {
      RELEASE(xname);
      RELEASE(xhost);
      WarnSQL(sql, "%s: %s", desctask(t), _("error deleting RRset via DNS UPDATE"));
      return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
    }
    while ((row = sql_getrow(res, &lengths))) {
      char *xdata = NULL, *xedata = NULL;
      int res2 = 0;
      xdata = sql_escstr2(sql, row[2], lengths[2]);
      if (mydns_rr_extended_data && lengths[5])
	xedata = sql_escstr2(sql, row[5], lengths[5]);
      querylen = sql_build_query(&query,
				 "DELETE FROM %s "
				 "WHERE zone=%u AND name='%s' AND type='%s' "
				 "AND data='%s' AND aux=%u AND ttl=%u "
				 "AND active='%s'%s%s%s",
				 mydns_rr_table_name,
				 soa->id, row[0], row[1],
				 xdata, atou(row[3]), atou(row[4]),
				 mydns_rr_active_types[2],
				 (xedata)?" AND edatakey='":"",
				 (xedata)?xedata:"",
				 (xedata)?"'":"");
      RELEASE(xdata);
      RELEASE(xedata);
      res2 = sql_nrquery(sql, query, querylen);
      RELEASE(query);
      if (res2 != 0) {
	WarnSQL(sql, "%s: %s - %s",
		desctask(t), _("error purging DELETE RR via DNS UPDATE"), query);
	RELEASE(xname);
	RELEASE(xhost);
	return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
      }
      updates++;
    }
    sql_free(res);
    /*
     * Phase 2 update all active records in the current RR set to be deleted
     */
    querylen = sql_build_query(&query,
			       "UPDATE %s SET active='%s',serial=%u "
			       "WHERE zone=%u AND (name='%s' OR name='%s') AND type='%s'",
			       mydns_rr_table_name, mydns_rr_active_types[2], next_serial,
			       soa->id, xname, xhost, mydns_qtype_str(rr->type));
  } else {
    updates++;
    querylen = sql_build_query(&query,
			       "DELETE FROM %s "
			       "WHERE zone=%u AND (name='%s' OR name='%s') AND type='%s'",
			       mydns_rr_table_name,
			       soa->id, xname, xhost, mydns_qtype_str(rr->type));
  }
#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: DELETE RRSET: %s"), desctask(t), query);
#else
#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
  DebugX("update", 1, _("%s: DNS UPDATE: %s"), desctask(t), query);
#endif
#endif
  RELEASE(xname);
  RELEASE(xhost);

  /* Execute the query */
  if (updates && sql_nrquery(sql, query, querylen) != 0) {
    WarnSQL(sql, "%s: %s", desctask(t), _("error deleting RRset via DNS UPDATE"));
    RELEASE(query);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
  }
  RELEASE(query);

  /* Output info to verbose log */
  { char *tmp;
    ASPRINTF(&tmp, "DELETE %s IN %s", UQRR_NAME(rr), mydns_qtype_str(rr->type));
    task_output_info(t, tmp);
    RELEASE(tmp);
  }
  if (updates) t->update_done++;

  return (TASK_EXECUTED);
}
/*--- update_delete_rr() ------------------------------------------------------------------------*/


/**************************************************************************************************
	PROCESS_UPDATE
	Perform the requested update.
	Returns 0 on success, -1 on failure.
**************************************************************************************************/
static taskexec_t
process_update(TASK *t, MYDNS_SOA *soa, UQ *q, UQRR *rr, uint32_t next_serial) {

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: process_update: q->name=[%s], q->type=%s, q->class=%s"), desctask(t),
	 UQ_NAME(q), mydns_qtype_str(q->type), mydns_class_str(q->class));
  DebugX("update", 1, _("%s: DNS UPDATE: process_update: UQRR_NAME(rr)=[%s], rr->type=%s, rr->class=%s"),
	 desctask(t),
	 UQRR_NAME(rr), mydns_qtype_str(rr->type), mydns_class_str(rr->class));
#endif

  /* 2.5.1: Add to an RRset */
  if (rr->class == q->class) {
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: 2.5.1: Add to an RRset"), desctask(t));
#endif
    return update_add_rr(t, soa, rr, next_serial);
  }

  /* 2.5.2: Delete an RRset */
  if (rr->type != DNS_CLASS_ANY && !UQRR_DATA_LENGTH(rr)) {
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: 2.5.2: Delete an RRset"), desctask(t));
#endif
    return update_delete_rrset(t, soa, rr, next_serial);
  }

  /* 2.5.3: Delete all RRsets from a name */
  if (rr->type == DNS_CLASS_ANY && !UQRR_DATA_LENGTH(rr)) {
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: 2.5.3: Delete all RRsets from a name"), desctask(t));
#endif
    return update_delete_rrset_all(t, soa, rr,next_serial);
  }

  /* 2.5.4: Delete an RR from an RRset */
  if (rr->type != DNS_CLASS_ANY && UQRR_DATA_LENGTH(rr)) {
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: 2.5.4: Delete an RR from an RRset"), desctask(t));
#endif
    return update_delete_rr(t, soa, rr, next_serial);
  }

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: process_update: no action"), desctask(t));
#endif

  return (TASK_EXECUTED);
}
/*--- process_update() --------------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_TMPRR
	Check the set of RRs described in q->tmprr -- each RRset must match exactly what's in the
	database, else we send NXRRSET.  AN "RRset" is described as an unique <NAME,TYPE>.
	Returns 0 on success, -1 on error.

	RFC 2136, 3.2.3 says:
	...build an RRset for each unique <NAME,TYPE> and compare each resulting RRset for set
	equality (same members, no more, no less) with RRsets in the zone.  If any Prerequisite
	RRset is not entirely and exactly matched by a zone RRset, signal NXRRSET to the requestor.
	If any RR in this section has a CLASS other than ZCLASS or NONE or ANY, signal FORMERR
	to the requestor.

	The temporary prerequisite RRsets are stored in q->tmprr (the count in q->num_tmprr).

	The algorithm used here is to loop through q->tmprr.
	The <NAME,TYPE> is inspected, and each RR with that <NAME,TYPE> is marked as
 	'tmprr->checked=1'.
	We then examine each <NAME,TYPE> of that sort in q->tmprr.
	Then, if any members of that <NAME,TYPE> are not matched, or if the count of records
	of that <NAME,TYPE> in the database does not match the number of records of that <NAME,TYPE>
	in q->tmprr, we return NXRRSET.

	The RFC isn't totally clear on AUX values, so I'm only checking AUX values on RR types where
	they ought to be relevant (currently MX and SRV).
**************************************************************************************************/
static taskexec_t
check_tmprr(TASK *t, MYDNS_SOA *soa, UQ *q) {
  int	n = 0, i = 0;

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: Checking prerequisite RRsets for exact match"), desctask(t));
#endif

  /* Examine "tmprr" */
  for (n = 0; n < q->num_tmprr; n++) {
    TMPRR	*tmprr = q->tmprr[n];
    uchar	*current_name = TMPRR_NAME(tmprr);	/* Current NAME being examined */
    dns_qtype_t	current_type = tmprr->type;		/* Current TYPE being examined */
    MYDNS_RR	*rr_first = NULL;			/* RRs for the current name/type */
    MYDNS_RR	*rr = NULL;				/* Current RR */
    int		total_prereq_rr = 0, total_db_rr = 0;	/* Total RRs in prereq and database */

    if (tmprr->checked) {				/* Ignore if already checked */
#if DEBUG_ENABLED && DEBUG_UPDATE
      DebugX("update", 1, _("%s: DNS UPDATE: Skipping prerequisite RRsets for %s/%s (already checked)"),
	     desctask(t),
	     current_name, mydns_qtype_str(current_type));
#endif
      continue;
    }

#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: Checking prerequisite RRsets for %s/%s"), desctask(t),
	   current_name, mydns_qtype_str(current_type));
#endif

    /* Load all RRs for this name/type */
    if (mydns_rr_load_active(sql, &rr_first, soa->id, current_type, (char*)current_name, NULL) != 0) {
      sql_reopen();
      if (mydns_rr_load_active(sql, &rr_first, soa->id, current_type, (char*)current_name, NULL) != 0) {
	WarnSQL(sql, _("error finding %s type resource records for name `%s' in zone %u"),
		mydns_qtype_str(current_type), current_name, soa->id);
	sql_reopen();
	return dnserror(t, DNS_RCODE_FORMERR, ERR_DB_ERROR);
      }
    }

    /* If no RRs were found, return NXRRSET */
    if (!rr_first) {
#if DEBUG_ENABLED && DEBUG_UPDATE
      DebugX("update", 1, _("%s: DNS UPDATE: Found prerequisite RRsets for %s/%s, but none in database (NXRRSET)"),
	     desctask(t), current_name, mydns_qtype_str(current_type));
#endif
      return dnserror(t, DNS_RCODE_NXRRSET, ERR_PREREQUISITE_FAILED);
    }

    /* Count the total number of RRs found in database */
    for (rr = rr_first; rr; rr = rr->next)
      total_db_rr++;
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: Found %d database RRsets for %s/%s"), desctask(t), total_db_rr,
	   current_name, mydns_qtype_str(current_type));
#endif

    /* Mark all <NAME,TYPE> matches in tmprr with checked=1, and count the number of RRs */
    for (i = 0; i < q->num_tmprr; i++)
      if (q->tmprr[i]->type == current_type && !strcasecmp((char*)TMPRR_NAME(q->tmprr[i]), (char*)current_name)) {
	q->tmprr[i]->checked = 1;
	total_prereq_rr++;
      }
#if DEBUG_ENABLED && DEBUG_UPDATE
    DebugX("update", 1, _("%s: DNS UPDATE: Found %d prerequisite RRsets for %s/%s"), desctask(t), total_prereq_rr,
	   current_name, mydns_qtype_str(current_type));
#endif

    /* If total_db_rr doesn't equal total_prereq_rr, return NXRRSET */
    if (total_db_rr != total_prereq_rr) {
#if DEBUG_ENABLED && DEBUG_UPDATE
      DebugX("update", 1,
	     _("%s: DNS UPDATE: Found %d prerequisite RRsets for %s/%s, but %d in database (NXRRSET)"),
	     desctask(t), total_prereq_rr, current_name, mydns_qtype_str(current_type), total_db_rr);
#endif
      mydns_rr_free(rr_first);
      return dnserror(t, DNS_RCODE_NXRRSET, ERR_PREREQUISITE_FAILED);
    }

    /* Also, for each matching <NAME,TYPE>, check to see if the record exists in the database.
       If it does, set matched=1.  If it does not, return NXRRSET */
    for (i = 0; i < q->num_tmprr; i++)
      if (q->tmprr[i]->type == current_type && !strcasecmp((char*)TMPRR_NAME(q->tmprr[i]), (char*)current_name)) {
	int found_match = 0;		/* Did we find a match for this RR? */

#if DEBUG_ENABLED && DEBUG_UPDATE
	DebugX("update", 1, _("%s: DNS UPDATE: looking for tmprr[%d] = %s/%s/%u/%s in database"), desctask(t),
	       i, TMPRR_NAME(q->tmprr[i]), mydns_qtype_str(q->tmprr[i]->type),
	       q->tmprr[i]->aux, TMPRR_DATA_VALUE(q->tmprr[i]));
#endif
	for (rr = rr_first; rr && !found_match; rr = rr->next) {
	  /* See if the DATA (and possibly the AUX) matches */
	  if ((MYDNS_RR_DATA_LENGTH(rr) == TMPRR_DATA_LENGTH(q->tmprr[i]))
	      && !memcmp(MYDNS_RR_DATA_VALUE(rr),
			 TMPRR_DATA_VALUE(q->tmprr[i]),
			 TMPRR_DATA_LENGTH(q->tmprr[i]))) {
	    if (current_type == DNS_QTYPE_MX || current_type == DNS_QTYPE_SRV) {
	      if (q->tmprr[i]->aux == rr->aux)
		found_match = 1;
	    } else {
	      found_match = 1;
	    }
	  }
	}

	/* No match found - return NXRRSET */
	if (!found_match) {
#if DEBUG_ENABLED && DEBUG_UPDATE
	  DebugX("update", 1, _("%s: DNS UPDATE: No match for prerequisite %s/%s/%u/%s (NXRRSET)"), desctask(t),
		 TMPRR_NAME(q->tmprr[i]), mydns_qtype_str(q->tmprr[i]->type),
		 q->tmprr[i]->aux, TMPRR_DATA_VALUE(q->tmprr[i]));
#endif
	  mydns_rr_free(rr_first);
	  return dnserror(t, DNS_RCODE_NXRRSET, ERR_PREREQUISITE_FAILED);
	}
      }
    mydns_rr_free(rr_first);
  }

  return (TASK_EXECUTED);
}
/*--- check_tmprr() -----------------------------------------------------------------------------*/

/**************************************************************************************************
	INCREMENT_SOA_SERIAL
	Increment the soa serial using the preferred syntax
**************************************************************************************************/
static uint32_t
increment_soa_serial(TASK *t, MYDNS_SOA *soa) {
  uint current_serial = soa->serial;
  uint new_serial = 0;
  char *datebuffer = NULL;

  struct timeval timenow = { 0, 0 };
  struct tm *local_current_time = NULL;

  time_t now = 0, low = 0, high = 0;
  unsigned int curyear  = 0;

  gettimeofday(&timenow, NULL);
  now = timenow.tv_sec;
  low = now - (31449600 * 2); /* About 2 years ago */
  high = now + (31449600 * 1); /* About a year away */

  local_current_time = gmtime((const time_t*)&timenow.tv_sec);

  curyear = local_current_time->tm_year + 1900;

  ASPRINTF(&datebuffer, "%04d%02d%02d%02d",
	   curyear,
	   local_current_time->tm_mon + 1,
	   local_current_time->tm_mday,
	   0);
  sscanf(datebuffer, "%d", &new_serial);
  RELEASE(datebuffer);

  if (!current_serial) {
    current_serial = new_serial;
  } else if (current_serial >= (uint)low && current_serial <= (uint)high) {
    if (current_serial == (uint)now) now++;
    current_serial = now;
  } else {
    unsigned int year = 0;
    ASPRINTF(&datebuffer, "%.12d", current_serial);
    datebuffer[4] = '\0';
    sscanf(datebuffer, "%d", &year);
    RELEASE(datebuffer);
    if (year >= (curyear - 10) && year <= (curyear +1)) {
      int n;
      for (n = 0; n < 3650; n++) {
	unsigned int rv;
	rv = new_serial + n;
	if (rv > current_serial) {
	  current_serial = rv;
	  break;
	}
      }
      if (n >= 3650)
	current_serial = new_serial + 1;
    } else if (current_serial < (uint)low) {
      current_serial += 1;
    } else if (new_serial > current_serial) {
      current_serial = new_serial;
    } else {
      current_serial += 1;
    }
  }

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: incremented soa from %d to %d"), desctask(t), soa->serial, current_serial);
#endif

  return current_serial;
}
/*--- increment_soa_serial() ---------------------------------------------------------------------*/

/**************************************************************************************************
	UPDATE_SOA_SERIAL
        Write the serial back to the database for this serial
**************************************************************************************************/
static taskexec_t
update_soa_serial(TASK *t, MYDNS_SOA *soa) {

  char		*query = NULL;
  size_t 	querylen = 0;

  SQL_RES 	*res = NULL;

  querylen = sql_build_query(&query, "UPDATE %s SET serial='%d' WHERE id='%d'",
			     mydns_soa_table_name, soa->serial, soa->id);

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: UPDATE SOA SERIAL: %s"), desctask(t), query);
#else
#if DEBUG_ENABLED && DEBUG_UPDATE_SQL
  DebugX("update", 1, "%s: DNS UPDATE: %s", desctask(t), query);
#endif
#endif
  if(sql_nrquery(sql, query, querylen) != 0) {
    WarnSQL(sql, "%s: %s", desctask(t), _("error updating soa serial via DNS UPDATE"));
    RELEASE(query);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
  }
  RELEASE(query);
  sql_free(res);

  ASPRINTF(&query, "UPDATE SOA %s SERIAL %d", soa->origin, soa->serial);
  task_output_info(t, query);
  RELEASE(query);
  t->update_done++;

  return (TASK_EXECUTED);
}
/*--- update_soa_serial() -----------------------------------------------------------------------*/

static int
are_we_master(TASK *t, MYDNS_SOA *soa) {
  ARRAY *ips4 = NULL;
#if HAVE_IPV6
  ARRAY *ips6 = NULL;
#endif

  char **allLocalAddresses = all_interface_addresses();
  int i = 0, j = 0, res = 1;

  int ipcount = 0;
  ARRAY *nss = array_init(1);

  array_append(nss, STRDUP(soa->ns));

  ips4 = array_init(8);
#if HAVE_IPV6
  ips6 = array_init(8);
#endif
  ipcount = name_servers2ip(t, nss, ips4,
#if HAVE_IPV6
			    ips6
#else
			    NULL
#endif
			    );
  /* name_server2ip frees this up - array_free(nss, 1); */

  if(array_numobjects(ips4)) {
    for (i = 0; i < array_numobjects(ips4); i++) {
      NOTIFYSLAVE *slave = array_fetch(ips4, i);
      struct sockaddr_in *ip4 = (struct sockaddr_in*)&(slave->slaveaddr);
      const char *ip4addr = ipaddr(AF_INET, &(ip4->sin_addr));
      for (j = 0; allLocalAddresses[j]; j++) {
	if(!strcmp(allLocalAddresses[j], ip4addr)) goto FINISHEDSEARCH;
      }
    }
  }

#if HAVE_IPV6
  if (array_numobjects(ips6)) {
    for (i = 0; i < array_numobjects(ips6); i++) {
      NOTIFYSLAVE *slave = array_fetch(ips6, i);
      struct sockaddr_in6 *ip6 = (struct sockaddr_in6*)&(slave->slaveaddr);
      const char *ip6addr = ipaddr(AF_INET6, &(ip6->sin6_addr));
      for (j = 0; allLocalAddresses[j]; j++) {
	if(!strcmp(allLocalAddresses[j], ip6addr)) goto FINISHEDSEARCH;
      }
    }
  }
#endif

  res = 0;

 FINISHEDSEARCH:

  array_free(ips4, 1);
#if HAVE_IPV4
  array_free(ips6, 1);
#endif

  return (res);
}
/**************************************************************************************************
	DNS_UPDATE
	Process a DNS UPDATE query.
**************************************************************************************************/
taskexec_t
dns_update(TASK *t) {
  MYDNS_SOA	*soa = NULL;						/* SOA record for zone */
  UQ		*q = NULL;						/* Update query data */
  int		n = 0;
  uint32_t	next_serial = 0;

  /* Try to load SOA for zone */
  if (mydns_soa_load(sql, &soa, t->qname) < 0) {
    dnserror(t, DNS_RCODE_SERVFAIL, ERR_DB_ERROR);
    return (TASK_FAILED);
  }

  /* If there's no such zone, say REFUSED rather than NOTAUTH, to prevent "zone mining" */
  if (!soa) {
    dnserror(t, DNS_RCODE_REFUSED, ERR_ZONE_NOT_FOUND);
    return (TASK_FAILED);
  }

#if DEBUG_ENABLED && DEBUG_UPDATE
  DebugX("update", 1, _("%s: DNS UPDATE: SOA id %u"), desctask(t), soa->id);
  DebugX("update", 1, _("%s: DNS UPDATE: ZOCOUNT=%d (Zone)"), desctask(t), t->qdcount);
  DebugX("update", 1, _("%s: DNS UPDATE: PRCOUNT=%d (Prerequisite)"), desctask(t), t->ancount);
  DebugX("update", 1, _("%s: DNS UPDATE: UPCOUNT=%d (Update)"), desctask(t), t->nscount);
  DebugX("update", 1, _("%s: DNS UPDATE: ADCOUNT=%d (Additional data)"), desctask(t), t->arcount);
#endif

  /*
   * Check that we are the master for this zone
   * i.e. one of our addresses matches the master record
   */
  if(!are_we_master(t, soa)) {
    dnserror(t, DNS_RCODE_NOTAUTH, ERR_NO_UPDATE);
    return (TASK_FAILED);
  }

  /* Check the optional 'update' column if it exists */
  if (check_update(t, soa) != 0) {
    return (TASK_FAILED);
  }

  /* Parse the update query */
  q = ALLOCATE(sizeof(UQ), UQ);
  if (parse_update_query(t, q) != TASK_EXECUTED) {
    goto dns_update_error;
  }

  /* Check the prerequsites as described in RFC 2136 3.2 */
  for (n = 0; n < q->numPR; n++)
    if (check_prerequisite(t, soa, q, &q->PR[n]) != TASK_EXECUTED) {
      goto dns_update_error;
    }

  /* Check the prerequisite RRsets -- RFC 2136 3.2.3 */
  if (check_tmprr(t, soa, q) != TASK_EXECUTED) {
    goto dns_update_error;
  }

  /* Prescan the update section (RFC 2136 3.4.1) */
  for (n = 0; n < q->numUP; n++)
    if (prescan_update(t, q, &q->UP[n]) != TASK_EXECUTED) {
      goto dns_update_error;
    }

  /* Process the update section (RFC 2136 3.4.2) */
  if (update_transaction(t, "BEGIN") != 0)	/* Start transaction */
    goto dns_update_error;
  /* Increment the serial on the SOA so that changes get stamped with the new one */
  next_serial = increment_soa_serial(t, soa);
  for (n = 0; n < q->numUP; n++) {
    if (process_update(t, soa, q, &q->UP[n], next_serial) != TASK_EXECUTED) {
      update_transaction(t, "ROLLBACK");	/* Rollback transaction */
      goto dns_update_error;
    }
  }
  if (t->update_done) {
    soa->serial = next_serial;
    if(update_soa_serial(t, soa) != TASK_EXECUTED) {
      update_transaction(t, "ROLLBACK");	/* Rollback transaction */
      goto dns_update_error;
    }
    if (update_transaction(t, "COMMIT") != 0)	/* Commit changes */
      goto dns_update_error;
    t->info_already_out = 1;

    /* Purge the cache for this zone */
    cache_purge_zone(ZoneCache, soa->id);
#if USE_NEGATIVE_CACHE
    cache_purge_zone(NegativeCache, soa->id);
#endif
    cache_purge_zone(ReplyCache, soa->id);

    /* Send out the notifications */
    notify_slaves(t, soa);
  } else {
    update_transaction(t, "ROLLBACK");
  }

  /* Construct reply and set task status */
  build_reply(t, 0);
  t->status = NEED_WRITE;

  /* Clean up and return */
  free_uq(q);
  mydns_soa_free(soa);
  return (TASK_EXECUTED);

dns_update_error:
  build_reply(t, 1);
  free_uq(q);
  mydns_soa_free(soa);
  return (TASK_FAILED);
}
/*--- dns_update() ------------------------------------------------------------------------------*/

/* vi:set ts=3: */
