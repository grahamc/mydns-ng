/**************************************************************************************************
	$Id: alias.c,v 1.15 2006/01/18 20:46:46 bboy Exp $

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
#define	DEBUG_ALIAS	1


#if ALIAS_ENABLED
/**************************************************************************************************
	FIND_ALIAS
	Find an ALIAS or A record for the alias.
	Returns the RR or NULL if not found.
**************************************************************************************************/
static MYDNS_RR *
find_alias(TASK *t, char *fqdn) {
  register MYDNS_SOA *soa = NULL;
  register MYDNS_RR *rr = NULL;
  register char *label = NULL;
  char *name = NULL;
	
  /* Load the SOA for the alias name. */

  if (!(soa = find_soa2(t, fqdn, &name)))
    return (NULL);

  /* Examine each label in the name, one at a time; look for relevant records */
  for (label = name; ; label++) {
    if (label == name || *label == '.') {
      if (label[0] == '.' && label[1]) label++;		/* Advance past leading dot */
#if DEBUG_ENABLED && DEBUG_ALIAS
      DebugX("alias", 1, _("%s: label=`%s'"), desctask(t), label);
#endif

      /* Do an exact match if the label is the first in the list */
      if (label == name) {
#if DEBUG_ENABLED && DEBUG_ALIAS
	DebugX("alias", 1, _("%s: trying exact match `%s'"), desctask(t), label);
#endif
	if ((rr = find_rr(t, soa, DNS_QTYPE_A, label))) {
	  RELEASE(name);
	  return (rr);
	}
      }

      /* No exact match. If the label isn't empty, replace the first part
	 of the label with `*' and check for wildcard matches. */
      if (*label) {
	MYDNS_SOA *zsoa = soa;
	int recurs = wildcard_recursion;
	do {
	  char *zlabel = label;

#if DEBUG_ENABLED && DEBUG_ALIAS
	  DebugX("alias", 1, _("%s: alias(%s) -> trying zone look up on %s - %d"),
		 desctask(t), label, zlabel, recurs);
#endif
	  /* Strip one label element and replace with a '*' then test and repeat until we run out of labels */
	  while (*zlabel) {
	    char *wclabel = NULL, *c = NULL;

	    /* Generate wildcarded label, i.e. `*.example' or maybe just `*'. */
	    if (!(c = strchr(label, '.')))
	      wclabel = STRDUP("*");
	    else
	      ASPRINTF(&wclabel, "*.%s", c);

#if DEBUG_ENABLED && DEBUG_ALIAS
	    DebugX("alias", 1, _("%s: alias(%s) trying wildcard `%s'"), desctask(t), label, wclabel);
#endif
	    rr = find_rr(t, zsoa, DNS_QTYPE_A, wclabel);
#if DEBUG_ENABLED && DEBUG_RESOLVE
	    DebugX("alias", 1, _("%s: resolve(%s) tried wildcard `%s' got rr = %p"),
		   desctask(t), label, wclabel, rr);
#endif
	    if(rr) {
	      RELEASE(name);
	      RELEASE(wclabel);
	      return (rr);
	    }
	    RELEASE(wclabel);
	    if (c) { zlabel = &c[1]; } else { break; }
	  }

#if DEBUG_ENABLED && DEBUG_ALIAS
	  DebugX("alias", 1, _("%s: alias(%s) -> shall we try recursive look up on - %d"),
		 desctask(t), label, recurs);
#endif
	  if (recurs--) {
#if DEBUG_ENABLED && DEBUG_ALIAS
	    DebugX("alias", 1, _("%s: alias(%s) -> trying recursive look up"), desctask(t), label);
#endif
	    char *zc;
	    if((zc = strchr(zsoa->origin, '.'))) {
	      zc = &zc[1];
	    } else {
	      goto NOWILDCARDMATCH;
	    }
	    if (*zc) {
	      MYDNS_SOA *xsoa;
#if DEBUG_ENABLED && DEBUG_ALIAS
	      DebugX("alias", 1, _("%s: alias(%s) -> trying recursive look up in %s"), desctask(t), label, zc);
#endif
	      xsoa = find_soa2(t, zc, NULL);
#if DEBUG_ENABLED && DEBUG_ALIAS
	      DebugX("alias", 1, _("%s: resolve(%s) -> got %s for recursive look up in %s"), desctask(t),
		    label, ((xsoa)?xsoa->origin:"<no match>"), zc);
#endif
	      if (xsoa) {
		/* Got a ancestor need to check that it is a parent for the last zone we checked */
#if DEBUG_ENABLED && DEBUG_ALIAS
		DebugX("alias", 1, _("%s: alias(%s) -> %s is an ancestor of %s"), desctask(t), label,
		      xsoa->origin, zsoa->origin);
#endif
		MYDNS_RR *xrr = find_rr(t, xsoa, DNS_QTYPE_NS, zsoa->origin);
#if DEBUG_ENABLED && DEBUG_ALIAS
		DebugX("alias", 1, _("%s: alias(%s) -> %s is%s a parent of %s"), desctask(t), label,
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

      NOWILDCARDMATCH:
	if (!*label)
	  break;
      }
    }
  }
  RELEASE(name);
  return (NULL);
}
/*--- find_alias() ------------------------------------------------------------------------------*/

/**************************************************************************************************
	ALIAS_RECURSE
	If the task has a matching ALIAS record, recurse into it.
	Returns the number of records added.
**************************************************************************************************/
int
alias_recurse(TASK *t, datasection_t section, char *fqdn, MYDNS_SOA *soa, char *label, MYDNS_RR *alias) {
  uint32_t		aliases[MAX_ALIAS_LEVEL];
  char			*name = NULL;
  register MYDNS_RR	*rr = NULL;
  register int		depth = 0, n = 0;

  memset(&aliases[0], 0, sizeof(aliases));

  if ((MYDNS_RR_DATA_LENGTH(alias) > 0)
      && (LASTCHAR((char*)MYDNS_RR_DATA_VALUE(alias)) != '.'))
    ASPRINTF(&name, "%s.%s", (char*)MYDNS_RR_DATA_VALUE(alias), soa->origin);
  else
    name = STRDUP((char*)MYDNS_RR_DATA_VALUE(alias));

  for (depth = 0; depth < MAX_ALIAS_LEVEL; depth++) {
#if DEBUG_ENABLED && DEBUG_ALIAS
    DebugX("alias", 1, _("%s: ALIAS -> `%s'"), desctask(t), name);
#endif
    /* Are there any alias records? */
    if ((rr = find_alias(t, name))) {
      /* We need an A record that is not an alias to end the chain. */
      if (rr->alias == 0) {
	/*
	** Override the id and name, because rrlist_add() checks for
	** duplicates and we might have several records aliased to one
	*/
	rr->id = alias->id;
	strcpy(MYDNS_RR_NAME(rr), MYDNS_RR_NAME(alias));
	rrlist_add(t, section, DNS_RRTYPE_RR, (void *)rr, fqdn);
	t->sort_level++;
	mydns_rr_free(rr);
	RELEASE(name);
	return (1);
      }

      /* Append origin if needed */
      if ((MYDNS_RR_DATA_LENGTH(rr) > 0)
	  && (LASTCHAR((char*)MYDNS_RR_DATA_VALUE(rr)) != '.')) {
	RELEASE(name);
	name = mydns_rr_append_origin((char*)MYDNS_RR_DATA_VALUE(rr), soa->origin);
      }

      /* Check aliases list; if we are looping, stop. Otherwise add this to the list. */
      for (n = 0; n < depth; n++)
	if (aliases[n] == rr->id) {
	  /* ALIAS loop: We aren't going to find an A record, so we're done. */
	  Verbose(_("%s: %s: %s (depth %d)"), desctask(t), _("ALIAS loop detected"), fqdn, depth);
	  mydns_rr_free(rr);
	  RELEASE(name);
	  return (0);
	}
      aliases[depth] = rr->id;

      /* Continue search with new alias. */
      strncpy(name, (char*)MYDNS_RR_DATA_VALUE(rr), sizeof(name)-1);
      mydns_rr_free(rr);
    } else {
      Verbose("%s: %s: %s -> %s", desctask(t), _("ALIAS chain is broken"), fqdn, name);
      RELEASE(name);
      return (0);
    }
  }
  Verbose(_("%s: %s: %s -> %s (depth %d)"), desctask(t), _("max ALIAS depth exceeded"),
	  fqdn, (char*)MYDNS_RR_DATA_VALUE(alias), depth);
  RELEASE(name);
  return (0);
}
/*--- alias_recurse() ---------------------------------------------------------------------------*/

#endif /* ALIAS_ENABLED */

/* vi:set ts=3: */
