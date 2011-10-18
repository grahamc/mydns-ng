/**************************************************************************************************
	$Id: sort.c,v 1.22 2006/01/18 20:46:47 bboy Exp $

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
#define	DEBUG_SORT	1

#define	RR_IS_RR(R)		((R)->rrtype == DNS_RRTYPE_RR)
#define	RR_IS_ADDR(R) 	(RR_IS_RR((R)) && (RR_IS_A((R)) || RR_IS_AAAA((R))))
#define	RR_IS_A(R)		(RR_IS_RR((R)) && (((MYDNS_RR *)(R)->rr)->type == DNS_QTYPE_A))
#define	RR_IS_AAAA(R)	(RR_IS_RR((R)) && (((MYDNS_RR *)(R)->rr)->type == DNS_QTYPE_AAAA))
#define	RR_IS_MX(R)		(RR_IS_RR((R)) && (((MYDNS_RR *)(R)->rr)->type == DNS_QTYPE_MX))
#define	RR_IS_SRV(R)	(RR_IS_RR((R)) && (((MYDNS_RR *)(R)->rr)->type == DNS_QTYPE_SRV))

#define	RAND(x)			((uint32_t)(((double)(x) + 1.0) * rand() / (RAND_MAX)))


/**************************************************************************************************
	SORTCMP
	Comparison function; sorts records:
		1. According to rr->sort_level
		2. According to rr->sort1
		3. According to rr->sort2
		4. According to rr->id
**************************************************************************************************/
static int
sortcmp(RR *rr1, RR *rr2) {
  if (rr1->sort_level != rr2->sort_level)
    return (rr1->sort_level - rr2->sort_level);

  if (rr1->sort1 != rr2->sort1)
    return (rr1->sort1 - rr2->sort1);

  if (rr1->sort2 != rr2->sort2)
    return (rr1->sort2 - rr2->sort2);

  return (rr1->id - rr2->id);
}
/*--- sortcmp() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	RRSORT
	Sorts the contents of an RR list according to the value(s) in 'sort'
**************************************************************************************************/
static RR *
rrsort(RR *head, int (*compar)(RR *, RR *)) {
  RR	*low = NULL, *high = NULL, *current = NULL, *pivot = NULL, *temp = NULL;
  int	result = 0;

  if (!head)
    return (NULL);
  current = head;
  do {
    current = current->next;
    if (!current)
      return (head);
  } while (!(result = compar(head,current)));

  if (result > 0)
    pivot = current;
  else
    pivot = head;
  low = high = NULL;
  current = head;
  while (current) {
    temp = current->next;
    if (compar(pivot,current) < 0) {
      current->next = high;
      high = current;
    } else {
      current->next = low;
      low = current;
    }
    current = temp;
  }
  low  = rrsort(low, compar);
  high = rrsort(high, compar);
  current = temp = low;
  while (1) {
    current = current->next;
    if (!current)
      break;
    temp = current;
  }
  temp->next = high;
  return low;
}
/*--- rrsort() ----------------------------------------------------------------------------------*/


/**************************************************************************************************
	SORT_RRLIST
	Sorts the specified RRLIST; reassigns tail node.
**************************************************************************************************/
static inline void
sort_rrlist(RRLIST *rrlist, int (*compar)(RR *, RR *)) {
  register RR *node = NULL;

  /* Do the sort */
  rrlist->head = rrsort(rrlist->head, compar);

  /* Redetermine list tail */
  for (node = rrlist->head; node; node = node->next)
    if (!node->next)
      rrlist->tail = node;
}
/*--- sort_rrlist() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	LOAD_BALANCE
	Use the 'aux' value to weight multiple A nodes.
**************************************************************************************************/
static inline void
load_balance(TASK *t, RRLIST *rrlist, datasection_t section, int sort_level) {
  register RR	*node = NULL;						/* Current node */
  register int order = 1;					/* Current order */

#if DEBUG_ENABLED && DEBUG_SORT
  DebugX("sort", 1, _("%s: Load balancing A records in %s section"), desctask(t), datasection_str[section]);
#endif

  /* Hosts with 'aux' values > 50000 are always listed last */
  for (node = rrlist->head; node; node = node->next)
    if (RR_IS_ADDR(node) && node->sort_level == sort_level && !node->sort1)
      if (((MYDNS_RR *)node->rr)->aux >= 50000)
	node->sort1 = 50000;

  for (;;) {
    register int found = 0;					/* Number of records with this aux */
    uint64_t	weights = 0;					/* Sum of aux */
    register uint32_t rweight = 0;				/* Random aux */

    /* Compute the sum of the weights for all nodes where 'sort1' == 0 */
    for (node = rrlist->head; node; node = node->next)
      if (RR_IS_ADDR(node) && node->sort_level == sort_level && !node->sort1) {
	found++;
	weights += ((MYDNS_RR *)node->rr)->aux;
      }
    if (!found)
      break;

    /* Set 'sort1' to 'order' for the first node found where the running sum
       value is greater than or equal to 'rweight' */
    rweight = RAND(weights);
    for (weights = 0, node = rrlist->head; node; node = node->next)
      if (RR_IS_ADDR(node) && node->sort_level == sort_level && !node->sort1) {
	weights += ((MYDNS_RR *)node->rr)->aux;
	if (weights >= rweight) {
	  node->sort1 = 65535 - order++;
	  break;
	}
      }
  }
}
/*--- load_balance() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	_SORT_A_RECS
	If the request is for 'A' or 'AAAA' and there are multiple A or AAAA records, sort them.
	Since this is an A or AAAA record, the answer section contains only addresses.
	If any of the RR's have nonzero "aux" values, do load balancing, else do round robin.
**************************************************************************************************/
static inline void
_sort_a_recs(TASK *t, RRLIST *rrlist, datasection_t section, int sort_level) {
  register RR *node = NULL;
  register int nonzero_aux = 0;
  register int count = 0;					/* Number of nodes at this level */

  /* If any addresses have nonzero 'aux' values, do load balancing */
  for (count = 0, node = rrlist->head; node; node = node->next)
    if (RR_IS_ADDR(node) && node->sort_level == sort_level) {
      count++;
      if (((MYDNS_RR *)node->rr)->aux)
	nonzero_aux = 1;
    }

  if (count < 2)						/* Only one node here, don't bother */
    return;
  t->reply_cache_ok = 0;					/* Don't cache load-balanced replies */

  if (nonzero_aux) {
    load_balance(t, rrlist, section, sort_level);
  } else {
    /* Round robin - for address records, set 'sort' to a random number */
#if DEBUG_ENABLED && DEBUG_SORT
    DebugX("sort", 1, _("%s: Sorting A records in %s section (round robin)"),
	   desctask(t), datasection_str[section]);
#endif

    for (node = rrlist->head; node; node = node->next)
      if (RR_IS_ADDR(node) && node->sort_level == sort_level)
	node->sort1 = RAND(4294967294U);
    t->reply_cache_ok = 0;					/* Don't cache load-balanced replies */
  }
}
/*--- _sort_a_recs() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	SORT_A_RECS
	If the request is for 'A' or 'AAAA' and there are multiple A or AAAA records, sort them.
	Since this is an A or AAAA record, the answer section contains only addresses.
	If any of the RR's have nonzero "aux" values, do load balancing, else do round robin.
**************************************************************************************************/
void
sort_a_recs(TASK *t, RRLIST *rrlist, datasection_t section) {
  register RR *node = NULL;

  /* Sort each sort level */
  for (node = rrlist->head; node; node = node->next)
    if (RR_IS_ADDR(node) && !node->sort2)
      _sort_a_recs(t, rrlist, section, node->sort_level);

  return (sort_rrlist(rrlist, sortcmp));
}
/*--- sort_a_recs() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	SORT_MX_RECS
	When there are multiple equal-preference MX records, randomize them to help keep the load
	equal.
**************************************************************************************************/
void
sort_mx_recs(TASK *t, RRLIST *rrlist, datasection_t section) {
  register RR *node = NULL;

#if DEBUG_ENABLED && DEBUG_SORT
  DebugX("sort", 1, _("%s: Sorting MX records in %s section"), desctask(t), datasection_str[section]);
#endif

  /* Set 'sort' to a random number */
  for (node = rrlist->head; node; node = node->next)
    if (RR_IS_MX(node)) {
      node->sort1 = ((MYDNS_RR *)node->rr)->aux;
      node->sort2 = RAND(4294967294U);
    }
  return (sort_rrlist(rrlist, sortcmp));
}
/*--- sort_mx_recs() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	SORT_SRV_PRIORITY
	Sorts one record for a single specified priority.
	After calling this function, 'sort2' should be 'weight' for the node affected.
	Returns the number of nodes processed (0 means "done with this priority").
**************************************************************************************************/
static inline int
sort_srv_priority(TASK *t, RRLIST *rrlist, datasection_t section, uint32_t priority,
		  int sort_level, int order) {
  register RR	*node = NULL;					/* Current node */
  register int found = 0;					/* Number of records with this priority */
  uint64_t	weights = 0;					/* Sum of weights */
  register uint32_t rweight = 0;				/* Random weight */

#if DEBUG_ENABLED && DEBUG_SORT
  DebugX("sort", 1, _("%s: Sorting SRV records in %s section with priority %u"),
	 desctask(t), datasection_str[section], priority);
#endif

  /* Compute the sum of the weights for all nodes with this priority where 'sort2' == 0 */
  for (node = rrlist->head; node; node = node->next)
    if (RR_IS_SRV(node) && node->sort_level == sort_level && node->sort1 == priority && !node->sort2) {
      found++;
      weights += MYDNS_RR_SRV_WEIGHT((MYDNS_RR *)node->rr);
    }
  if (!found)
    return (0);

  /* Set 'sort2' to 'order' for the first node found at this priority where the running sum
     value is greater than or equal to 'rweight' */
  rweight = RAND(weights+1);
  for (weights = 0, node = rrlist->head; node; node = node->next)
    if (RR_IS_SRV(node) && node->sort_level == sort_level
	&& node->sort1 == priority && !node->sort2) {
      weights += MYDNS_RR_SRV_WEIGHT((MYDNS_RR *)node->rr);
      if (weights >= rweight) {
	node->sort2 = order;
	return (1);
      }
    }
  return (1);
}
/*--- sort_srv_priority() -----------------------------------------------------------------------*/


/**************************************************************************************************
	_SORT_SRV_RECS
	Sorts SRV records at the specified sort level.
	1. Sort by priority, lowest to highest.
	2. Sort by weight; 0 means "almost never choose me", higher-than-zero yields
		increased likelihood of being first.
**************************************************************************************************/
static inline void
_sort_srv_recs(TASK *t, RRLIST *rrlist, datasection_t section, int sort_level) {
  register RR	*node = NULL;						/* Current node */
  register int count = 0;						/* Number of SRV nodes on this level */

#if DEBUG_ENABLED && DEBUG_SORT
  DebugX("sort", 1, _("%s: Sorting SRV records in %s section"), desctask(t), datasection_str[section]);
#endif

  /* Assign 'sort1' to the priority (aux) and 'sort2' to 0 if there's a zero weight, else random */
  for (count = 0, node = rrlist->head; node; node = node->next)
    if (RR_IS_SRV(node) && node->sort_level == sort_level) {
      count++;
      node->sort1 = ((MYDNS_RR *)node->rr)->aux;
      if (MYDNS_RR_SRV_WEIGHT((MYDNS_RR *)node->rr) == 0)
	node->sort2 = 0;
      else
	node->sort2 = RAND(4294967294U);
    }

  if (count < 2)						/* Only one node here, don't bother */
    return;
  t->reply_cache_ok = 0;					/* Don't cache these replies */

  /* Sort a first time, so that the list is ordered by priority/weight */
  sort_rrlist(rrlist, sortcmp);

  /* Reset 'sort2' to zero for each SRV */
  for (node = rrlist->head; node; node = node->next)
    if (RR_IS_SRV(node) && node->sort_level == sort_level)
      node->sort2 = 0;

  /* For each unique priority, sort by weight */
  for (node = rrlist->head; node; node = node->next)
    if (RR_IS_SRV(node) && node->sort_level == sort_level && !node->sort2) {
      register int priority = node->sort1;
      register int order = 1;

      while (sort_srv_priority(t, rrlist, section, priority, sort_level, order++))
	/* DONOTHING */;
    }
}
/*--- _sort_srv_recs() --------------------------------------------------------------------------*/


/**************************************************************************************************
	SORT_SRV_RECS
**************************************************************************************************/
void
sort_srv_recs(TASK *t, RRLIST *rrlist, datasection_t section) {
  register RR	*node = NULL;						/* Current node */

  /* Sort each sort level */
  for (node = rrlist->head; node; ) {
    /* _sort_srv_recs() will sort the list, so we need to reset the list each time */
    if (RR_IS_SRV(node) && !node->sort2) {
      _sort_srv_recs(t, rrlist, section, node->sort_level);
      node = rrlist->head;
    } else
      node = node->next;
  }
}
/*--- sort_srv_recs() ---------------------------------------------------------------------------*/

/* vi:set ts=3: */
