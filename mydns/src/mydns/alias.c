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
MYDNS_RR *
find_alias(TASK *t, char *fqdn)
{
	register MYDNS_SOA *soa;
	register MYDNS_RR *rr;
	register char *label;
	char name[DNS_MAXNAMELEN+1];
	
	/* Load the SOA for the alias name. */
	memset(name, 0, sizeof(name));
	if (!(soa = find_soa(t, fqdn, name)))
		return (NULL);

	/* Examine each label in the name, one at a time; look for relevant records */
	for (label = name; ; label++)
	{
		if (label == name || *label == '.')
		{
			if (label[0] == '.' && label[1]) label++;		/* Advance past leading dot */
#if DEBUG_ENABLED && DEBUG_ALIAS
			Debug("%s: label=`%s'", desctask(t), label);
#endif

			/* Do an exact match if the label is the first in the list */
			if (label == name)
			{
#if DEBUG_ENABLED && DEBUG_ALIAS
				Debug("%s: trying exact match `%s'", desctask(t), label);
#endif
				if ((rr = find_rr(t, soa, DNS_QTYPE_A, label)))
					return (rr);
			}

			/* No exact match. If the label isn't empty, replace the first part
				of the label with `*' and check for wildcard matches. */
			if (*label)
			{
				uchar wclabel[DNS_MAXNAMELEN+1], *c;

				/* Generate wildcarded label, i.e. `*.example' or maybe just `*'. */
				if (!(c = strchr(label, '.')))
					wclabel[0] = '*', wclabel[1] = '\0';
				else
					wclabel[0] = '*', strncpy(wclabel+1, c, sizeof(wclabel)-2);

#if DEBUG_ENABLED && DEBUG_ALIAS
				Debug("%s: trying wildcard `%s'", desctask(t), wclabel);
#endif
				if ((rr = find_rr(t, soa, DNS_QTYPE_A, wclabel)))
					return (rr);
			}
		}
		if (!*label)
			break;
	}
	return (NULL);
}
/*--- find_alias() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	ALIAS_RECURSE
	If the task has a matching ALIAS record, recurse into it.
	Returns the number of records added.
**************************************************************************************************/
int
alias_recurse(TASK *t, datasection_t section, char *fqdn, MYDNS_SOA *soa, char *label, MYDNS_RR *alias)
{
	uint32_t aliases[MAX_ALIAS_LEVEL];
	char name[DNS_MAXNAMELEN+1];
	register MYDNS_RR *rr;
	register int depth, n;

	if (LASTCHAR(alias->data) != '.')
		snprintf(name, sizeof(name), "%s.%s", alias->data, soa->origin);
	else
		strncpy(name, alias->data, sizeof(name)-1);

	for (depth = 0; depth < MAX_ALIAS_LEVEL; depth++)
	{
#if DEBUG_ENABLED && DEBUG_ALIAS
		Debug("%s: ALIAS -> `%s'", desctask(t), name);
#endif
		/* Are there any alias records? */
		if ((rr = find_alias(t, name)))
		{
			/* We need an A record that is not an alias to end the chain. */
			if (rr->alias == 0)
			{
				/* Override the id and name, because rrlist_add() checks for duplicates and we might have several records aliased to one */
				rr->id = alias->id;
				strcpy(rr->name, alias->name);
				rrlist_add(t, section, DNS_RRTYPE_RR, (void *)rr, fqdn);
				t->sort_level++;
				mydns_rr_free(rr);
				return (1);
			}

			/* Append origin if needed */
			int len = strlen(rr->data);
			if (len > 0 && rr->data[len - 1] != '.') {
				strcat(rr->data, ".");
				strncat(rr->data, soa->origin, sizeof(rr->name) - len - 1);
			}

			/* Check aliases list; if we are looping, stop. Otherwise add this to the list. */
			for (n = 0; n < depth; n++)
				if (aliases[n] == rr->id)
				{
					/* ALIAS loop: We aren't going to find an A record, so we're done. */
					Verbose("%s: %s: %s (depth %d)", desctask(t), _("ALIAS loop detected"), fqdn, depth);
					mydns_rr_free(rr);
					return (0);
				}
			aliases[depth] = rr->id;

			/* Continue search with new alias. */
			strncpy(name, rr->data, sizeof(name)-1);
			mydns_rr_free(rr);
		}
		else
		{
			Verbose("%s: %s: %s -> %s", desctask(t), _("ALIAS chain is broken"), fqdn, name);
			return (0);
		}
	}
	Verbose("%s: %s: %s -> %s (depth %d)", desctask(t), _("max ALIAS depth exceeded"), fqdn, alias->data, depth);
	return (0);
}
/*--- alias_recurse() ---------------------------------------------------------------------------*/

#endif /* ALIAS_ENABLED */

/* vi:set ts=3: */
