/**************************************************************************************************
	$Id: resolve.c,v 1.59 2006/01/18 20:46:47 bboy Exp $

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

/* Make this nonzero to enable debugging for this source file */
#define	DEBUG_RESOLVE	1

#if DEBUG_ENABLED
/* Strings describing the datasections */
const char *resolve_datasection_str[] = { "QUESTION", "ANSWER", "AUTHORITY", "ADDITIONAL" };
#endif

/**************************************************************************************************
	RESOLVE_SOA
	Adds SOA record to the specified section.  If that section is ANSWER, assume this is a SOA
	query.  Returns number of records inserted, or -1 if an error occurred.
**************************************************************************************************/
static taskexec_t
resolve_soa(TASK *t, datasection_t section, char *fqdn, int level) {
  MYDNS_SOA *soa = find_soa2(t, fqdn, NULL);

#if DEBUG_ENABLED && DEBUG_RESOLVE
  DebugX("resolve", 1, _("%s: resolve_soa(%s) -> soa %s"), desctask(t), fqdn, (soa)?soa->origin:_("not found"));
#endif

  /* Resolve with this record as the FQDN */
  if (soa) {
    /* Condition mimics code from resolve BUT check for forwarding recursive first
       then if zone has been marked as recursive
       then if section is ANSWER and we are in level 0 */
    if(soa->recursive) {
      if(forward_recursive && t->hdr.rd)
	if((section == ANSWER) && !level)
	  return recursive_fwd(t);
      return dnserror(t, DNS_RCODE_REFUSED, ERR_ZONE_NOT_FOUND);
    }

    t->zone = soa->id;
    t->minimum_ttl = soa->minimum;

    /* Don't cache replies for SOA with a 0 second TTL */
    if (!soa->minimum && !soa->ttl)
      t->reply_cache_ok = 0;

    /* This is a SOA request - handle things a tiny bit differently */
    if (t->qtype == DNS_QTYPE_SOA && section == ANSWER)	{
      /* If the fqdn does not exactly match, put the SOA in AUTHORITY instead of ANSWER */
      if (strcmp(fqdn, soa->origin)) {
	rrlist_add(t, AUTHORITY, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
	t->sort_level++;
      }	else {
	/* SOA in ANSWER - also add authoritative nameservers */
	rrlist_add(t, section, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
	t->sort_level++;
	(void)resolve(t, AUTHORITY, DNS_QTYPE_NS, soa->origin, 0);
      }
    } else {
      rrlist_add(t, section, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
      t->sort_level++;
    }

    mydns_soa_free(soa);

    if (section == ANSWER)				/* We are authoritative; Set `aa' flag */
      t->hdr.aa = 1;
    return (TASK_EXECUTED);
  }
  return (section == ANSWER ? dnserror(t, DNS_RCODE_REFUSED, ERR_ZONE_NOT_FOUND) : TASK_EXECUTED);
}
/*--- resolve_soa() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	CNAME_RECURSE
	If task has a dominant matching CNAME record, recurse into it.
	Returns the number of records added.
**************************************************************************************************/
static taskexec_t
cname_recurse(TASK *t, datasection_t section, dns_qtype_t qtype,
	      char *fqdn, MYDNS_RR *cname, int level) {
  register int n = 0;

  if (level >= MAX_CNAME_LEVEL)
    return (1);

  /* Add the CNAME record to the answer section */
  if (section == ANSWER && level)
    rrlist_add(t, ADDITIONAL, DNS_RRTYPE_RR, (void *)cname, fqdn);
  else
    rrlist_add(t, section, DNS_RRTYPE_RR, (void *)cname, fqdn);
  t->sort_level++;

  /* If the request was for CNAME records, this is the answer; we are done. */
  if (t->qtype == DNS_QTYPE_CNAME)
    return (TASK_COMPLETED);

  /* Check `Cnames' list; if we are looping, stop.  Otherwise add this to the array. */
  for (n = 0; n < level; n++)
    if (t->Cnames[n] == cname->id) {
      /* CNAME loop: Send what we have so far and consider the resolution complete */
      Verbose(_("%s: %s: %s %s %s (depth %d)"), desctask(t), _("CNAME loop detected"),
	      MYDNS_RR_NAME(cname), mydns_qtype_str(cname->type), (char*)MYDNS_RR_DATA_VALUE(cname), level);
      return (TASK_COMPLETED);
    }
  t->Cnames[level] = cname->id;

#if DEBUG_ENABLED && DEBUG_RESOLVE
  DebugX("resolve", 1, _("%s: CNAME -> `%s'"), desctask(t), (char*)MYDNS_RR_DATA_VALUE(cname));
#endif

  /* Resolve with this new CNAME record as the FQDN */
  return resolve(t, section, qtype, MYDNS_RR_DATA_VALUE(cname), level+1);
}
/*--- cname_recurse() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	PROCESS_RR
	Process the resource record list.  Returns number of records added.
**************************************************************************************************/
static taskexec_t
process_rr(TASK *t, datasection_t section, dns_qtype_t qtype, char *fqdn,
	   MYDNS_SOA *soa, char *label, MYDNS_RR *rr, int level) {
  register MYDNS_RR *r = NULL;
  register int rv = 0;
  register int add_ns = (section == ANSWER
			 && !t->ns.size
			 && qtype != DNS_QTYPE_NS
			 && qtype != DNS_QTYPE_ANY);

  t->name_ok = 1;

  /* If the data section calls for a FQDN, and we just get a hostname, append the origin */
  for (r = rr; r; r = r->next) {
    if (r->type == DNS_QTYPE_NS || r->type == DNS_QTYPE_CNAME || r->type == DNS_QTYPE_MX) {
      mydns_rr_data_append_origin(r, soa->origin);
    }
  }

  /* If the RR list returned contains a CNAME record, follow the CNAME. */
  for (r = rr; r; r = r->next)
    if (r->type == DNS_QTYPE_CNAME)
      return cname_recurse(t, section, qtype, fqdn, r, level);

  /* Find RRs matching QTYPE */
  for (r = rr; r; r = r->next)
    if (r->type == qtype || qtype == DNS_QTYPE_ANY) {
#if ALIAS_ENABLED
      /* If the RR is an ALIAS then follow it, otherwise just add it. */
      if (r->alias)
	rv += alias_recurse(t, section, fqdn, soa, label, r);
      else {
	rrlist_add(t, section, DNS_RRTYPE_RR, (void *)r, fqdn);
	rv++;
      }
#else
      rrlist_add(t, section, DNS_RRTYPE_RR, (void *)r, fqdn);
      rv++;
#endif
    }
  t->sort_level++;

  /* If we found no matching RR's but there are NS records, and the name isn't empty
     or '*' treat as delegation -- set 'rv'
     to make the caller return, then set 'add_ns' to fill the AUTHORITY */
  if (!rv && *label && *label != '*')
    for (r = rr; !rv && r; r = r->next)
      if (r->type == DNS_QTYPE_NS)
	rv = add_ns = 1;

  /* If we found some results, go ahead and put nameserver records into AUTHORITY */
  if (rv) {
    for (r = rr; r; r = r->next) {
      if (r->type == DNS_QTYPE_NS && add_ns) {
	char *ns = NULL;

	/* If the rr is for something like "*.bboy.net.", show the labelized name */
	if (MYDNS_RR_NAME(r)[0] == '*' && MYDNS_RR_NAME(r)[1] == '.' && MYDNS_RR_NAME(r)[2])
	  ASPRINTF(&ns, "%s.%s", MYDNS_RR_NAME(r)+2, soa->origin);
	else if (MYDNS_RR_NAME(r)[0] && MYDNS_RR_NAME(r)[0] != '*')
	  ASPRINTF(&ns, "%s.%s", MYDNS_RR_NAME(r), soa->origin);
	else
	  ns = STRDUP(soa->origin);

	rrlist_add(t, AUTHORITY, DNS_RRTYPE_RR, (void *)r, ns);
	RELEASE(ns);

	/* If the NS data is a FQDN, look in THIS zone for an A record.  That way glue
	   records can be stored out of baliwick a la BIND */
	if (LASTCHAR((char*)MYDNS_RR_DATA_VALUE(r)) == '.') {
	  MYDNS_RR *A = find_rr(t, soa, DNS_QTYPE_A, MYDNS_RR_DATA_VALUE(r));

	  if (A) {
	    register MYDNS_RR *a;

	    for (a = A; a; a = a->next)
	      rrlist_add(t, ADDITIONAL, DNS_RRTYPE_RR, (void *)a, MYDNS_RR_DATA_VALUE(r));
	    mydns_rr_free(A);
	  }
	}
	rv++;
      }
    }
  }
  t->sort_level++;

  /* We DID find matches for this label; thus, reply success but with no records in the
     ANSWER section. */
  if (!rv && section == ANSWER) {
    if (!t->ns.size)
      rrlist_add(t, AUTHORITY, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
    rv++;
    t->sort_level++;
  }

  return ((rv) ? TASK_COMPLETED : TASK_EXECUTED);
}
/*--- process_rr() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	ADD_AUTHORITY_NS
	Adds AUTHORITY records for any NS records that match the request.
**************************************************************************************************/
static void
add_authority_ns(TASK *t, datasection_t section, MYDNS_SOA *soa, char *match_label) {
  if (!t->ns.size && section == ANSWER)	{
    register MYDNS_RR *rr = NULL, *r = NULL;
    register char *label = NULL;

    /* Match down label by label in `label' -- include first matching NS record(s) */
    for (label = match_label; *label; label++) {
      if (label == match_label || *label == '.') {
	if (label[0] == '.' && label[1]) label++;		/* Advance past leading dot */

	/* Ignore NS records on wildcard */
	if (*label != '*') {
	  if ((rr = find_rr(t, soa, DNS_QTYPE_NS, label))) {
	    for (r = rr; r; r = r->next) {
	      char *name = NULL;

	      ASPRINTF(&name, "%s.%s", label, soa->origin);
	      rrlist_add(t, AUTHORITY, DNS_RRTYPE_RR, (void *)r, name);
	      RELEASE(name);
	    }
	    t->sort_level++;
	    mydns_rr_free(rr);
	    return;
	  }
	}
      }
    }

    /* Nothing added - try empty label */
    if ((rr = find_rr(t, soa, DNS_QTYPE_NS, label))) {
      for (r = rr; r; r = r->next)
	rrlist_add(t, AUTHORITY, DNS_RRTYPE_RR, (void *)r, soa->origin);
      t->sort_level++;
      mydns_rr_free(rr);
    }
  }
}
/*--- add_authority_ns() ------------------------------------------------------------------------*/


/**************************************************************************************************
	RESOLVE_LABEL
	Examine `label' (or a wildcard).  Add any relevant records.
	Returns nonzero if resolution is complete (because matches were found), else 0.
**************************************************************************************************/
static taskexec_t
resolve_label(TASK *t, datasection_t section, dns_qtype_t qtype,
	      char *fqdn, MYDNS_SOA *soa, char *label, int level) {
  register MYDNS_RR	*rr = NULL;
  taskexec_t		rv = 0;
  int			recurs = wildcard_recursion;
  char                  *savelabel = label;

#if DEBUG_ENABLED && DEBUG_RESOLVE
  DebugX("resolve", 1, _("%s: resolve_label(%s, %s, %s, %s, %d)"), desctask(t),
	 fqdn, soa->origin, label, mydns_qtype_str(qtype), level);
#endif

  /* Do any records match this label exactly? */
  rr = find_rr(t, soa, DNS_QTYPE_ANY, label);

  if (rr) {
    rv = process_rr(t, section, qtype, fqdn, soa, label, rr, level);
    mydns_rr_free(rr);
    add_authority_ns(t, section, soa, label);
#if DEBUG_ENABLED && DEBUG_RESOLVE
    DebugX("resolve", 1, _("%s: resolve_label(%s) returning results %s"), desctask(t),
	   fqdn, task_exec_name(rv));
#endif
    return (rv);
  }

  /* No exact match */
  /* Check for NS delegation */
  while (*label) {
    if ((rr = find_rr(t, soa, DNS_QTYPE_NS, label))) {
      char *newfqdn;
      if (LASTCHAR(label) == '.') {
	newfqdn = STRDUP(label);
      } else {
	ASPRINTF(&newfqdn, "%s.%s", label, soa->origin);
      }
      rv = process_rr(t, AUTHORITY, qtype, newfqdn, soa, label, rr, level);
      mydns_rr_free(rr);
      RELEASE(newfqdn);
      add_authority_ns(t, section, soa, label);
#if DEBUG_ENABLED && DEBUG_RESOLVE
      DebugX("resolve", 1, _("%s: resolve_label(%s) returning results %s"), desctask(t),
	     fqdn, task_exec_name(rv));
#endif
      return (rv);
    }
    label = strchr(label, '.');
    if (!label) break;
    label++;
  }
  /* No NS delegation. */
  /* Restore label, it may have been modified above */
  label = savelabel;

  /*
   * No exact match.
   * If `label' isn't empty, replace the first part of the label with `*'
   * and check for wildcard matches.
   * Search up the ancestor path until the recursion count expires
   * or we find a missing zone (zone for which we do not hold the master SOA)
   * or we get a match.
   */
  if (*label) {
    MYDNS_SOA *zsoa = soa;
    do {
      char *zlabel = label;

#if DEBUG_ENABLED && DEBUG_RESOLVE
      DebugX("resolve", 1, _("%s: resolve_label(%s) -> trying zone look up on %s - %d"),
	     desctask(t), label, zlabel, recurs);
#endif
      /* Strip one label element and replace with a '*' then test and repeat until we run out of labels */
      while (*zlabel) {
	char *wclabel = NULL, *c = NULL;

	/* Generate wildcarded label, i.e. `*.example' or maybe just `*'. */
	if (!(c = strchr(zlabel, '.')))
	  wclabel = STRDUP("*");
	else
	  ASPRINTF(&wclabel, "*%s", c);

#if DEBUG_ENABLED && DEBUG_RESOLVE
	DebugX("resolve", 1, _("%s: resolve_label(%s) trying wildcard `%s'"), desctask(t), label, wclabel);
#endif
	rr = find_rr(t, zsoa, DNS_QTYPE_ANY, wclabel);
#if DEBUG_ENABLED && DEBUG_RESOLVE
	DebugX("resolve", 1, _("%s: resolve(%s) tried wildcard `%s' got rr = %p"),
	       desctask(t), label, wclabel, rr);
#endif
	if (rr) {
	  rv = process_rr(t, section, qtype, fqdn, zsoa, wclabel, rr, level);
	  mydns_rr_free(rr);
	  add_authority_ns(t, section, zsoa, wclabel);
#if DEBUG_ENABLED && DEBUG_RESOLVE
	  DebugX("resolve", 1, _("%s: resolve_label(%s) returning results %s having matched %s"), desctask(t),
		 fqdn, task_exec_name(rv), wclabel);
#endif
	  RELEASE(wclabel);
	  return (rv);
	}
	RELEASE(wclabel);
	if (c) { zlabel = &c[1]; } else { break; }
      }

#if DEBUG_ENABLED && DEBUG_RESOLVE
      DebugX("resolve", 1, _("%s: resolve_label(%s) -> shall we try recursive look up - %d"),
	     desctask(t), label, recurs);
#endif
      if (recurs--) {
#if DEBUG_ENABLED && DEBUG_RESOLVE
	DebugX("resolve", 1, _("%s: resolve_label(%s) -> trying recursive look up"), desctask(t), label);
#endif
	/* Find the parent zone that has the current zone delegated and try in there */
	char *zc;
	if((zc = strchr(zsoa->origin, '.'))) {
	  zc = &zc[1];
	} else {
	  goto NOWILDCARDMATCH;
	}
	if (*zc) {
	  MYDNS_SOA *xsoa;
#if DEBUG_ENABLED && DEBUG_RESOLVE
	  DebugX("resolve", 1, _("%s: resolve_label(%s) -> trying recursive look up in %s"),
		 desctask(t), label, zc);
#endif
	  xsoa = find_soa2(t, zc, NULL);
#if DEBUG_ENABLED && DEBUG_RESOLVE
	  DebugX("resolve", 1, _("%s: resolve_label(%s) -> got %s for recursive look up in %s"),
		 desctask(t), label, ((xsoa)?xsoa->origin:"<no match>"), zc);
#endif
	  if (xsoa) {
	    /* Got a ancestor need to check that it is a parent for the last zone we checked */
#if DEBUG_ENABLED && DEBUG_RESOLVE
	    DebugX("resolve", 1, _("%s: resolve_label(%s) -> %s is an ancestor of %s"), desctask(t), label,
		   xsoa->origin, zsoa->origin);
#endif
	    MYDNS_RR *xrr = find_rr(t, xsoa, DNS_QTYPE_NS, zsoa->origin);
#if DEBUG_ENABLED && DEBUG_RESOLVE
	    DebugX("resolve", 1, _("%s: resolve_label(%s) -> %s is%s a parent of %s"), desctask(t), label,
		   ((xrr) ? "" : " not"),
		   xsoa->origin, zsoa->origin);
#endif
	    if (xrr) {
	      mydns_rr_free(xrr);
	      if (zsoa != soa) {
		mydns_soa_free(zsoa);
	      }
	      zsoa = xsoa;
	    } else {
	      goto NOWILDCARDMATCH;
	    }
	  }
	} else {
	  goto NOWILDCARDMATCH;
	}
      } else {
	goto NOWILDCARDMATCH;
      }
    } while (1);
  }

 NOWILDCARDMATCH:
  return (TASK_EXECUTED);
}
/*--- resolve_label() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	RESOLVE
	Resolves the specified name, storing all data found in the specified section.
	If `section' is ANSWER, this function will set an error if we lack an authoritative answer.
	Returns number of records inserted, or -1 if an error occurred.
**************************************************************************************************/
taskexec_t
resolve(TASK *t, datasection_t section, dns_qtype_t qtype, char *fqdn, int level) {
  char			*name = NULL;
  register MYDNS_SOA	*soa = NULL;
  taskexec_t		rv = TASK_COMPLETED;

#if DEBUG_ENABLED && DEBUG_RESOLVE
  DebugX("resolve", 1, _("%s: resolve(%s, %s, \"%s\", %d)"),
	 desctask(t), resolve_datasection_str[section], mydns_qtype_str(qtype), fqdn, level);
#endif

#if STATUS_ENABLED
  if (t->qclass == DNS_CLASS_CHAOS)
    return remote_status(t);
#endif

    if (!axfr_enabled && (t->qtype == DNS_QTYPE_AXFR || t->qtype == DNS_QTYPE_IXFR))

    return dnserror(t, DNS_RCODE_REFUSED, ERR_NO_AXFR);

  /* Is the request for a SOA record only? */
  if (t->qtype == DNS_QTYPE_SOA && section == ANSWER)
    return resolve_soa(t, section, fqdn, level);

  /*
  ** Load SOA record for this name - if section is ANSWER and no SOA is found, we're not
  ** authoritative
  ** Go recursive if the SOA is so labelled
  */
  soa = find_soa2(t, fqdn, &name);

#if DEBUG_ENABLED && DEBUG_RESOLVE
  DebugX("resolve", 1, _("%s: resolve(%s) -> soa %s"), desctask(t), fqdn, (soa)?soa->origin:_("not found"));
#endif

  if (!soa || soa->recursive) {
    RELEASE(name);
    if ((section == ANSWER) && !level) {
#if DEBUG_ENABLED && DEBUG_RESOLVE
      DebugX("resolve", 1, _("%s: Checking for recursion soa = %p, soa->recursive = %d, "
			     "forward_recursive = %d, t->hdr.rd = %d, section = %d, level = %d"),
	     desctask(t),
	     soa, (soa) ? (int)soa->recursive : -1,
	     forward_recursive, t->hdr.aa, section, level);
#endif
      if (forward_recursive && t->hdr.rd) {
	return recursive_fwd(t);
      }
      return dnserror(t, DNS_RCODE_REFUSED, ERR_ZONE_NOT_FOUND);
    } else {
      return TASK_CONTINUE;
    }
  }

  t->zone = soa->id;
  t->minimum_ttl = soa->minimum;

  /* We are authoritative; Set `aa' flag */
  if (section == ANSWER)
    t->hdr.aa = 1;

  /* If the request is ANY, and `fqdn' exactly matches the origin, include SOA */
  if ((qtype == DNS_QTYPE_ANY) && (section == ANSWER) && !strcasecmp(fqdn, soa->origin)) {
    rrlist_add(t, section, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
    t->sort_level++;
  }

  /******* NEW_RESOLVER ***************/
  /*
  ** Written 28th February 2008 to fix wildcard look ups
  ** and make the handling of glue more understandable.
  */

  /*
  ** The SOA we have in our hand 'encloses' the label in this server
  ** it may however be resident in a zone provided by another server
  **
  ** Therefore we need to check whether the residual name has any
  ** NS records which would reference it in the zone we hold.
  **
  ** So we need to find matching glue records as the first step.
  **
  */

  /*
  ** Look for the full label first as an exact match in the current zone.
  */
  rv = resolve_label(t, section, qtype, fqdn, soa, name, level);

#if DEBUG_ENABLED && DEBUG_RESOLVE
  DebugX("resolve", 1, _("%s: resolve(%s) -> trying `%s', %s"), desctask(t), fqdn, name, task_exec_name(rv));
#endif

  RELEASE(name);

  /* If we got this far and there are NO records, set result and send the SOA */
  if (!level && !t->an.size && !t->ns.size && !t->ar.size) {
    if (t->name_ok) {
      t->hdr.rcode = DNS_RCODE_NOERROR;
      t->reason = ERR_NONE;
    } else {
      t->hdr.rcode = DNS_RCODE_NXDOMAIN;
      t->reason = ERR_NO_MATCHING_RECORDS;
    }
    rrlist_add(t, AUTHORITY, DNS_RRTYPE_SOA, (void *)soa, soa->origin);
    t->sort_level++;
  }

  mydns_soa_free(soa);
  
  return (rv);
}	
/*--- resolve() ---------------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
