/**************************************************************************************************
	$Id: reply.c,v 1.65 2006/01/18 20:46:47 bboy Exp $

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

#include "memoryman.h"

#include "encode.h"
#include "reply.h"
#include "rr.h"

#include "sort.h"

/* Make this nonzero to enable debugging for this source file */
#define	DEBUG_REPLY	1

#if DEBUG_ENABLED && DEBUG_REPLY
/* Strings describing the datasections */
char *reply_datasection_str[] = { "QUESTION", "ANSWER", "AUTHORITY", "ADDITIONAL" };
#endif


/**************************************************************************************************
	REPLY_PROCESS_RRLIST
	Adds each resource record found in `rrlist' to the reply.
**************************************************************************************************/
static int
reply_process_rrlist(TASK *t, RRLIST *rrlist) {
  register RR *r = NULL;

  if (!rrlist)
    return (0);

  for (r = rrlist->head; r; r = r->next) {
    dns_qtype_map *map;
    switch (r->rrtype) {
    case DNS_RRTYPE_SOA:
      map = mydns_rr_get_type_by_id(DNS_QTYPE_SOA);
      if (map->rr_reply_add(t, r, map) < 0)
	return (-1);
      break;

    case DNS_RRTYPE_RR:
      {
	MYDNS_RR *rr = (MYDNS_RR *)r->rr;

	if (!rr) break;

	map = mydns_rr_get_type_by_id(rr->type);
	if (map->rr_reply_add(t, r, map) < 0)
	  return -1;

      }
      break;
    }
  }
  return (0);
}
/*--- reply_process_rrlist() --------------------------------------------------------------------*/


/**************************************************************************************************
	TRUNCATE_RRLIST
	Returns new count of items in this list.
	The TC flag is _not_ set if data was truncated from the ADDITIONAL section.
**************************************************************************************************/
static int
truncate_rrlist(TASK *t, off_t maxpkt, RRLIST *rrlist, datasection_t ds) {
  register RR *rr = NULL;
  register int recs = 0;
#if DEBUG_ENABLED && DEBUG_REPLY
  int orig_recs = rrlist->size;
#endif

  /* Warn about truncated packets, but only if TCP is not enabled.  Most resolvers will try
     TCP if a UDP packet is truncated. */
  if (!tcp_enabled)
    Verbose("%s: %s", desctask(t), _("query truncated"));

  recs = rrlist->size;
  for (rr = rrlist->head; rr; rr = rr->next) {
    if (rr->offset + rr->length >= maxpkt) {
      recs--;
      if (ds != ADDITIONAL)
	t->hdr.tc = 1;
    } else
      t->rdlen += rr->length;
  }
#if DEBUG_ENABLED && DEBUG_REPLY
  DebugX("buildreply", 1, _("%s section truncated from %d records to %d records"),
	 reply_datasection_str[ds], orig_recs, recs);
#endif
  return (recs);
}
/*--- truncate_rrlist() -------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_CHECK_TRUNCATION
	If this reply would be truncated, removes any RR's that won't fit and sets the truncation flag.
**************************************************************************************************/
static void
reply_check_truncation(TASK *t, int *ancount, int *nscount, int *arcount) {
  size_t maxpkt = (t->protocol == SOCK_STREAM ? DNS_MAXPACKETLEN_TCP : DNS_MAXPACKETLEN_UDP);
  size_t maxrd = maxpkt - (DNS_HEADERSIZE + t->qdlen);

  if (t->rdlen <= maxrd)
    return;

#if DEBUG_ENABLED && DEBUG_REPLY
  DebugX("buildreply", 1, _("reply_check_truncation() needs to truncate reply (%u) to fit packet max (%u)"),
	 (unsigned int)t->rdlen, (unsigned int)maxrd);
#endif

  /* Loop through an/ns/ar sections, truncating as necessary, and updating counts */
  t->rdlen = 0;
  *ancount = truncate_rrlist(t, maxpkt, &t->an, ANSWER);
  *nscount = truncate_rrlist(t, maxpkt, &t->ns, AUTHORITY);
  *arcount = truncate_rrlist(t, maxpkt, &t->ar, ADDITIONAL);
}
/*--- reply_check_truncation() ------------------------------------------------------------------*/

/**************************************************************************************************
	BUILDREPLY_INIT
	Examines the question data, storing the name offsets (from DNS_HEADERSIZE) for compression.
**************************************************************************************************/
int buildreply_init(TASK *t) {
  register char *c = NULL;						/* Current character in name */

  /* Examine question data, save labels found therein. The question data should begin with
     the name we've already parsed into t->qname.  I believe it is safe to assume that no
     compression will be possible in the question. */
  for (c = t->qname; *c; c++)
    if ((c == t->qname || *c == '.') && c[1])
      if (name_remember(t, (c == t->qname) ? c : (c+1),
			(((c == t->qname) ? c : (c+1)) - t->qname) + DNS_HEADERSIZE) < -1)
	return (-1);
  return (0);
}
/*--- reply_init() ------------------------------------------------------------------------------*/




void buildreply_abandon(TASK *t) {
  /* Empty RR lists */
  rrlist_free(&t->an);
  rrlist_free(&t->ns);
  rrlist_free(&t->ar);

  /* Make sure reply is empty */
  t->replylen = 0;
  t->rdlen = 0;
  RELEASE(t->rdata);
}

/**************************************************************************************************
	BUILDREPLY_CACHE
	Builds reply data from cached answer.
**************************************************************************************************/
void buildreply_cache(TASK *t) {
  char *dest = t->reply;

  DNS_PUT16(dest, t->id);							/* Query ID */
  DNS_PUT(dest, &t->hdr, SIZE16);						/* Header */
}
/*--- buildreply_cache() -----------------------------------------------------------------------*/


/**************************************************************************************************
	BUILDREPLY
	Given a task, constructs the reply data.
**************************************************************************************************/
void buildreply(TASK *t, int want_additional) {
  char	*dest = NULL;
  int	ancount = 0, nscount = 0, arcount = 0;

  /* Add data to ADDITIONAL section */
  if (want_additional) {
    mydns_reply_add_additional(t, &t->an, ANSWER);
    mydns_reply_add_additional(t, &t->ns, AUTHORITY);
  }

  /* Sort records where necessary */
  if (t->an.a_records > 1)			/* ANSWER section: Sort A/AAAA records */
    sort_a_recs(t, &t->an, ANSWER);
  if (t->an.mx_records > 1)			/* ANSWER section: Sort MX records */
    sort_mx_recs(t, &t->an, ANSWER);
  if (t->an.srv_records > 1)			/* ANSWER section: Sort SRV records */
    sort_srv_recs(t, &t->an, ANSWER);
  if (t->ar.a_records > 1)			/* AUTHORITY section: Sort A/AAAA records */
    sort_a_recs(t, &t->ar, AUTHORITY);

  /* Build `rdata' containing resource records in ANSWER, AUTHORITY, and ADDITIONAL */
  t->replylen = DNS_HEADERSIZE + t->qdlen + t->rdlen;
  if (reply_process_rrlist(t, &t->an)
      || reply_process_rrlist(t, &t->ns)
      || reply_process_rrlist(t, &t->ar)) {
    buildreply_abandon(t);
  }

  ancount = t->an.size;
  nscount = t->ns.size;
  arcount = t->ar.size;

  /* Verify reply length */
  reply_check_truncation(t, &ancount, &nscount, &arcount);

  /* Make sure header bits are set correctly */
  t->hdr.qr = 1;
  t->hdr.cd = 0;

  /* Construct the reply */
  t->replylen = DNS_HEADERSIZE + t->qdlen + t->rdlen;
  dest = t->reply = ALLOCATE(t->replylen, char[]);

  DNS_PUT16(dest, t->id);					/* Query ID */
  DNS_PUT(dest, &t->hdr, SIZE16);				/* Header */
  DNS_PUT16(dest, t->qdcount);					/* QUESTION count */
  DNS_PUT16(dest, ancount);					/* ANSWER count */
  DNS_PUT16(dest, nscount);					/* AUTHORITY count */
  DNS_PUT16(dest, arcount);					/* ADDITIONAL count */
  if (t->qdlen && t->qd)
    DNS_PUT(dest, t->qd, t->qdlen);				/* Data for QUESTION section */
  DNS_PUT(dest, t->rdata, t->rdlen);				/* Resource record data */

#if DEBUG_ENABLED && DEBUG_REPLY
  DebugX("buildreply", 1, _("%s: reply:     id = %u"), desctask(t),
	 t->id);
  DebugX("buildreply", 1, _("%s: reply:     qr = %u (message is a %s)"), desctask(t),
	 t->hdr.qr, t->hdr.qr ? "response" : "query");
  DebugX("buildreply", 1, _("%s: reply: opcode = %u (%s)"), desctask(t),
	 t->hdr.opcode, mydns_opcode_str(t->hdr.opcode));
  DebugX("buildreply", 1, _("%s: reply:     aa = %u (answer %s)"), desctask(t),
	 t->hdr.aa, t->hdr.aa ? "is authoritative" : "not authoritative");
  DebugX("buildreply", 1, _("%s: reply:     tc = %u (message %s)"), desctask(t),
	 t->hdr.tc, t->hdr.tc ? "truncated" : "not truncated");
  DebugX("buildreply", 1, _("%s: reply:     rd = %u (%s)"), desctask(t),
	 t->hdr.rd, t->hdr.rd ? "recursion desired" : "no recursion");
  DebugX("buildreply", 1, _("%s: reply:     ra = %u (recursion %s)"), desctask(t),
	 t->hdr.ra, t->hdr.ra ? "available" : "unavailable");
  DebugX("buildreply", 1, _("%s: reply:  rcode = %u (%s)"), desctask(t),
	 t->hdr.rcode, mydns_rcode_str(t->hdr.rcode));
  /* escdata(t->reply, t->replylen); */
#endif
}
/*--- buildreply() -----------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
