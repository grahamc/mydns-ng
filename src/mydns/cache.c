/**************************************************************************************************
	$Id: cache.c,v 1.105 2006/01/18 20:46:46 bboy Exp $

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
#define	DEBUG_CACHE	1

/* Set this to nonzero to debug frequency of SQL queries */
#define	DEBUG_SQL_QUERIES 1

CACHE *ZoneCache = NULL;							/* Data cache */
CACHE *ReplyCache = NULL;							/* Reply cache */

#if USE_NEGATIVE_CACHE
CACHE *NegativeCache = NULL;						/* Negative zone cache */
#endif

#ifdef DN_COLUMN_NAMES
extern char	*dn_default_ns;						/* Default NS for directNIC */
#endif


#if (HASH_TYPE == ORIGINAL_HASH) || (HASH_TYPE == ADDITIVE_HASH)
/**************************************************************************************************
	ISPRIME
	Returns 1 if `number' is a prime number, 0 if not.
**************************************************************************************************/
static int
isprime(unsigned int number) {
  register unsigned int divn = 3;

  while (divn * divn < number && number % divn != 0)
    divn += 2;
  return (number % divn != 0);
}
/*--- isprime() ---------------------------------------------------------------------------------*/
#endif


/**************************************************************************************************
	_CACHE_INIT
	Create, initialize, and return a new CACHE structure.
**************************************************************************************************/
static CACHE *
_cache_init(uint32_t limit, uint32_t expire, const char *desc) {
  CACHE *C = NULL;

  C = ALLOCATE(sizeof(CACHE), CACHE);
  C->limit = limit;
  C->expire = expire;

#if (HASH_TYPE == ORIGINAL_HASH) || (HASH_TYPE == ADDITIVE_HASH)
  /* Make `slots' prime */
  C->slots = limit * CACHE_SLOT_MULTIPLIER;
  C->slots |= 1;
  while (!isprime(C->slots))
    C->slots += 2;
#elif (HASH_TYPE == ROTATING_HASH) || (HASH_TYPE == FNV_HASH)
  /* Make `slots' a power of two */
  {
    int		bits = 0;
    uint32_t	slots = 0;

    C->slots = limit * CACHE_SLOT_MULTIPLIER;
    for (slots = C->slots, bits = 0; slots != 1; ) {
      slots >>= 1;
      bits++;
    }
    if (C->slots & ((1 << bits) - 1))
      bits++;
    slots = 1 << bits;

    C->slots = slots;
#if (HASH_TYPE == ROTATING_HASH)
    C->mask = C->slots - 1;
#endif
#if (HASH_TYPE == FNV_HASH)
    /* A 16-bit hash lets us use XOR instead of MOD to clamp the value - very fast */
    if (C->slots < 65536)
      C->slots = 65536;
    for (C->bits = 0; C->bits < 32; C->bits++)
      if (((uint32_t)1 << C->bits) == C->slots)
	break;
#endif
  }
#else
#	error Hash method unknown or unspecified
#endif

  C->nodes = ALLOCATE_N(C->slots, sizeof(CNODE *), CNODE *);

#if DEBUG_ENABLED && DEBUG_CACHE
#if (HASH_TYPE == ORIGINAL_HASH)
  DebugX("cache", 1, _("%s cache initialized (%u nodes, %u elements max) (original hash)"),
	 desc, C->slots, limit);
#elif (HASH_TYPE == ADDITIVE_HASH)
  DebugX("cache", 1, _("%s cache initialized (%u nodes, %u elements max) (additive hash)"),
	 desc, C->slots, limit);
#elif (HASH_TYPE == ROTATING_HASH)
  DebugX("cache", 1, _("%s cache initialized (%u nodes, %u elements max) (rotating hash)"),
	 desc, C->slots, limit);
#elif (HASH_TYPE == FNV_HASH)
  DebugX("cache", 1, _("%s cache initialized (%u nodes, %u elements max) (%d-bit FNV hash)"),
	 desc, C->slots, limit,
	 C->bits);
#else
#	error Hash method unknown or unspecified
#endif
#endif

  strncpy(C->name, desc, sizeof(C->name)-1);
  return (C);
}
/*--- _cache_init() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	CACHE_INIT
	Create the caches used by MyDNS.
**************************************************************************************************/
void
cache_init(void) {
  uint32_t	cache_size = 0, zone_cache_size = 0, reply_cache_size = 0;
  int		defaulted = 0;
  int		zone_cache_expire = 0, reply_cache_expire = 0;

  /* Get ZoneCache size */
  zone_cache_size = atou(conf_get(&Conf, "zone-cache-size", &defaulted));
  if (defaulted) {
    cache_size = atou(conf_get(&Conf, "cache-size", NULL));
    zone_cache_size = cache_size - (cache_size / 3);
  }
  zone_cache_expire = atou(conf_get(&Conf, "zone-cache-expire", &defaulted));
  if (defaulted)
    zone_cache_expire = atou(conf_get(&Conf, "cache-expire", NULL));

  /* Get ReplyCache size */
  reply_cache_size = atou(conf_get(&Conf, "reply-cache-size", &defaulted));
  if (defaulted) {
    cache_size = atou(conf_get(&Conf, "cache-size", NULL));
    reply_cache_size = cache_size / 3;
  }
  reply_cache_expire = atou(conf_get(&Conf, "reply-cache-expire", &defaulted));
  if (defaulted)
    reply_cache_expire = atou(conf_get(&Conf, "cache-expire", NULL)) / 2;

  /* Initialize caches */
  if (zone_cache_size)
    ZoneCache = _cache_init(zone_cache_size, zone_cache_expire, "zone");
#if USE_NEGATIVE_CACHE
  if (zone_cache_size)
    NegativeCache = _cache_init(zone_cache_size, zone_cache_expire, "negative");
#endif
  if (reply_cache_size)
    ReplyCache = _cache_init(reply_cache_size, reply_cache_expire, "reply");
}
/*--- cache_init() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	CACHE_SIZE_UPDATE
	Updates the 'size' variable in a cache.
**************************************************************************************************/
static void
cache_size_update(CACHE *C) {
  register uint n = 0;
  register CNODE *N = NULL;

  C->size = 0;
  C->size += sizeof(CACHE);

  /* Get size of all data in cache */
  for (n = 0; n < C->slots; n++)
    for (N = C->nodes[n]; N; N = N->next_node) {
      C->size += sizeof(CNODE);

      if (C == ZoneCache) {
	if (N->data) {
	  if (N->type == DNS_QTYPE_SOA)
	    C->size += mydns_soa_size((MYDNS_SOA *)N->data);
	  else
	    C->size += mydns_rr_size((MYDNS_RR *)N->data);
	}
      } else 
	C->size += N->datalen;
    }
}
/*--- cache_size_update() -----------------------------------------------------------------------*/


/**************************************************************************************************
	CACHE_STATUS
	Called when SIGUSR1 is received, returns a string to append to status.
**************************************************************************************************/
void
cache_status(CACHE *C) {
  if (C) {
    register uint ct = 0, collisions = 0;
    register CNODE *n = NULL;

    /* Update cache size (bytes) */
    cache_size_update(C);

    /* Count number of collisions */
    for (ct = collisions = 0; ct < C->slots; ct++)
      if (C->nodes[ct] && C->nodes[ct]->next_node)
	for (n = C->nodes[ct]->next_node; n; n = n->next_node)
	  collisions++;

    Notice(_("%s cache %.0f%% useful (%u hits, %u misses),"
	     " %u collisions (%.0f%%), %.0f%% full (%u records), %u bytes, avg life %u sec"),
	   C->name, PCT(C->questions, C->hits), C->hits, C->misses,
	   collisions, PCT(C->slots, collisions),
	   PCT(C->limit, C->count), (uint)C->count, (uint)C->size,
	   (uint)(C->removed
		  ? (uint)C->removed_secs / C->removed
		  : (uint)(time(NULL) - Status.start_time))
	   );
  }
}
/*--- cache_status() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	MRULIST_ADD
	Adds the specified node to the head of the MRU list.
**************************************************************************************************/
static void
mrulist_add(CACHE *ThisCache, CNODE *n) {
  register CNODE *head = ThisCache->mruHead;

  if (!n || !ThisCache) return;

  if (!ThisCache->mruHead) {
    ThisCache->mruHead = n;
    ThisCache->mruHead->mruPrev = NULL;
    ThisCache->mruHead->mruNext = NULL;
    ThisCache->mruTail = n;
  } else {
    n->mruNext = head;
    n->mruPrev = head->mruPrev;
    if (head->mruPrev == NULL)
      ThisCache->mruHead = n;
    else
      head->mruPrev->mruNext = n;
    head->mruPrev = n;
  }
}
/*--- mrulist_add() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	MRULIST_DEL
**************************************************************************************************/
static void
mrulist_del(CACHE *ThisCache, CNODE *n) {
  if (!n || !ThisCache || !ThisCache->mruHead) return;

  if (n == ThisCache->mruHead) {
    ThisCache->mruHead = n->mruNext;
    if (ThisCache->mruHead == NULL)
      ThisCache->mruTail = NULL;
    else
      n->mruNext->mruPrev = NULL;
  } else {
    n->mruPrev->mruNext = n->mruNext;
    if (n->mruNext == NULL)
      ThisCache->mruTail = n->mruPrev;
    else
      n->mruNext->mruPrev = n->mruPrev;
  }
}
/*--- mrulist_del() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	CACHE_FREE_NODE
	Frees the node specified and removes it from the cache.
**************************************************************************************************/
static void
cache_free_node(CACHE *ThisCache, uint32_t hash, CNODE *n) {
  register CNODE *prev = NULL, *cur = NULL, *next = NULL;

  if (!n || hash >= ThisCache->slots)
    return;

  for (cur = ThisCache->nodes[hash]; cur; cur = next) {
    next = cur->next_node;
    if (cur == n) {						/* Delete this node */
      mrulist_del(ThisCache, n);				/* Remove from MRU/LRU list */

      /* Remove the node */
      if (cur->datalen) {
	RELEASE(cur->data);
      }	else if (cur->type == DNS_QTYPE_SOA) {
	mydns_soa_free(cur->data);
      } else {
	mydns_rr_free(cur->data);
      }

      if (cur == ThisCache->nodes[hash])			/* Head of node? */
	ThisCache->nodes[hash] = cur->next_node;
      else if (prev)
	prev->next_node = cur->next_node;
      RELEASE(cur);
      ThisCache->out++;
      ThisCache->count--;
      return;
    } else
      prev = cur;
  }
  Errx(_("tried to free invalid node %p at %u in cache"), n, hash);
}
/*--- cache_free_node() -------------------------------------------------------------------------*/


/**************************************************************************************************
	CACHE_EMPTY
	Deletes all nodes within the cache.
**************************************************************************************************/
void
cache_empty(CACHE *ThisCache) {
  register uint		ct = 0;
  register CNODE	*n = NULL, *tmp = NULL;

  if (!ThisCache) return;
  for (ct = 0; ct < ThisCache->slots; ct++)
    for (n = ThisCache->nodes[ct]; n; n = tmp) {
      tmp = n->next_node;
      cache_free_node(ThisCache, ct, n);
    }
  ThisCache->mruHead = ThisCache->mruTail = NULL;
}
/*--- cache_empty() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	CACHE_CLEANUP
	Deletes all expired nodes within the cache.
**************************************************************************************************/
void
cache_cleanup(CACHE *ThisCache) {
  register uint		ct = 0;
  register CNODE	*n = NULL, *tmp = NULL;

  if (!ThisCache)
    return;
  for (ct = 0; ct < ThisCache->slots; ct++)
    for (n = ThisCache->nodes[ct]; n; n = tmp) {
      tmp = n->next_node;
      if (n->expire && (current_time > n->expire)) {
	ThisCache->expired++;
	cache_free_node(ThisCache, ct, n);
      }
    }
}
/*--- cache_cleanup() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	CACHE_PURGE_ZONE
	Deletes all nodes within the cache for the specified zone.
**************************************************************************************************/
void
cache_purge_zone(CACHE *ThisCache, uint32_t zone) {
  register uint		ct = 0;
  register CNODE	*n = NULL, *tmp = NULL;

  if (!ThisCache)
    return;
  for (ct = 0; ct < ThisCache->slots; ct++)
    for (n = ThisCache->nodes[ct]; n; n = tmp) {
      tmp = n->next_node;
      if (n->zone == zone)
	cache_free_node(ThisCache, ct, n);
    }
}
/*--- cache_purge_zone() ------------------------------------------------------------------------*/


/**************************************************************************************************
	CACHE_HASH
	Returns hash value.
**************************************************************************************************/
static inline uint32_t
cache_hash(CACHE *ThisCache, uint32_t initval, void *buf, register size_t buflen) {
  unsigned char *bufp = buf;
#if (HASH_TYPE == ORIGINAL_HASH)
  register uint32_t	hash = 0;
  register unsigned char *p = NULL;

  /* Generate hash value */
  for (hash = initval, p = bufp; p < (bufp + buflen); p++) {
    register int tmp = 0;
    hash = (hash << 4) + (*p);
    if ((tmp = (hash & 0xf0000000))) {
      hash = hash ^ (tmp >> 24);
      hash = hash ^ tmp;
    }
  }
  return (hash % ThisCache->slots);
#elif (HASH_TYPE == ADDITIVE_HASH)
  register uint32_t	hash = 0;
  register unsigned char *p = NULL;

  for (hash = initval, p = bufp; p < (bufp + buflen); p++)
    hash += *p;
  return (hash % ThisCache->slots);
#elif (HASH_TYPE == ROTATING_HASH)
  register uint32_t	hash = 0;
  register unsigned char *p = NULL;

  for (hash = initval, p = buf; p < (buf + buflen); p++)
    hash = (hash << 4) ^ (hash >> 28) ^ (*p);
  return ((hash ^ (hash>>10) ^ (hash>>20)) & ThisCache->mask);
#elif (HASH_TYPE == FNV_HASH)
  register uint32_t	hash = FNV_32_INIT;
  unsigned char *bp = bufp;
  unsigned char *be = bp + buflen;

  while (bp < be) {
    hash *= FNV_32_PRIME;
    hash ^= (uint32_t)*bp++;
  }
  hash *= FNV_32_PRIME;
  hash ^= (uint32_t)initval;

  if (ThisCache->bits == 16)
    return ((hash >> 16) ^ (hash & (((uint32_t)1 << 16) - 1)));
  else
    return (hash % ThisCache->slots);
#else
#	error Hash method unknown or unspecified
#endif
}
/*--- cache_hash() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	ZONE_CACHE_FIND
	Returns the SOA/RR from cache (or via the database) or NULL if `name' doesn't match.
**************************************************************************************************/
void *
zone_cache_find(TASK *t, uint32_t zone, char *origin, dns_qtype_t type,
		const char *name, size_t namelen, int *errflag, MYDNS_SOA *parent) {
  register uint32_t	hash = 0;
  register CNODE	*n = NULL;
  MYDNS_SOA		*soa = NULL;
  MYDNS_RR		*rr = NULL;
  CACHE			*C = NULL;			/* Which cache to use when inserting */

  *errflag = 0;

  if (!name)
    return (NULL);

#if DEBUG_ENABLED && DEBUG_CACHE
  DebugX("cache", 1, _("%s: zone_cache_find(%d, %s, %s, %s, %d, %u, %p)"), desctask(t), zone, origin,
	 mydns_qtype_str(type), name, (unsigned int)namelen, *errflag, parent);
#endif

  if (ZoneCache) {
    hash = cache_hash(ZoneCache, zone + type, (void*)name, namelen);
#if USE_NEGATIVE_CACHE
    /* Check negative reply cache */
    if (NegativeCache) {
      NegativeCache->questions++;

      /* Look at the appropriate node.  Descend list and find match. */
      for (n = NegativeCache->nodes[hash]; n; n = n->next_node)
	if ((n->namelen == namelen) && (n->zone == zone) && (n->type == type)) {
	  if (!n->name)
	    Errx(_("negative cache node %p at hash %u has NULL name"), n, hash);
	  if (!memcmp(n->name, name, namelen)) {
	    /* Is the node expired? */
	    if (n->expire && (current_time > n->expire)) {
	      cache_free_node(NegativeCache, hash, n);
	      break;
	    }
	    NegativeCache->hits++;

	    /* Found in cache; move to head of usefulness list */
	    mrulist_del(NegativeCache, n);
	    mrulist_add(NegativeCache, n);
	    return NULL;
	  }
	}
      NegativeCache->misses++;
    }
#endif

    /* Not in negative cache, so look in zone cache */
    ZoneCache->questions++;

    /* Look at the appropriate node.  Descend list and find match. */
    for (n = ZoneCache->nodes[hash]; n; n = n->next_node) {
      if ((n->namelen == namelen) && (n->zone == zone) && (n->type == type)) {
	if (!n->name)
	  Errx(_("zone cache node %p at hash %u has NULL name"), n, hash);
	if (!memcmp(n->name, name, namelen)) {
	  /* Is the node expired? */
	  if (n->expire && (current_time > n->expire)) {
	    cache_free_node(ZoneCache, hash, n);
	    break;
	  }
	  ZoneCache->hits++;

	  /* Found in cache; move to head of usefulness list */
	  mrulist_del(ZoneCache, n);
	  mrulist_add(ZoneCache, n);
	  if (type == DNS_QTYPE_SOA)
	    return (n->data ? mydns_soa_dup(n->data, 1) : NULL);
	  else
	    return (n->data ? mydns_rr_dup(n->data, 1) : NULL);
	}
      }
    }
  }

  /* Result not found in cache; Get answer from database */
  if (type == DNS_QTYPE_SOA) {
    /* Try to load from database */
#if DEBUG_ENABLED && DEBUG_SQL_QUERIES
    DebugX("cache", 1, _("%s: SQL query: table \"%s\", origin=\"%s\""), desctask(t), mydns_soa_table_name, name);
#endif
    if (mydns_soa_load(sql, &soa, name) != 0) {
      sql_reopen();
      if (mydns_soa_load(sql, &soa, name) != 0) {
	WarnSQL(sql, "%s: %s", name, _("error loading SOA"));
	*errflag = 1;
	return (NULL);
      }
    }

#ifdef DN_COLUMN_NAMES
    if (soa && dn_default_ns)
      strncpy(soa->ns, dn_default_ns, sizeof(soa->ns)-1);
#endif

#if USE_NEGATIVE_CACHE
    if (!(C = soa ? ZoneCache : NegativeCache))
#else
      if (!(soa && (C = ZoneCache)))
#endif
	return ((void *)soa);

    /* Don't cache if TTL is 0 */
    if (soa && !soa->ttl)
      return ((void *)soa);
  } else {
#if DEBUG_ENABLED && DEBUG_SQL_QUERIES
    DebugX("cache", 1, _("%s: SQL query: table \"%s\", zone=%u,type=\"%s\",name=\"%s\""),
	   desctask(t), mydns_rr_table_name, zone, mydns_qtype_str(type), name);
#endif
    if (mydns_rr_load_active(sql, &rr, zone, type, name, origin) != 0) {
      sql_reopen();
      if (mydns_rr_load_active(sql, &rr, zone, type, name, origin) != 0) {
	WarnSQL(sql, _("error finding %s type resource records for name `%s' in zone %u"),
		mydns_qtype_str(type), name, zone);
	sql_reopen();
	*errflag = 1;
	return (NULL);
      }
    }

#ifdef DN_COLUMN_NAMES
    /* DN database has no TTL - use parent's */
    if (rr && parent && parent->ttl)
      rr->ttl = parent->ttl;
#endif

#if USE_NEGATIVE_CACHE
    if (!(C = rr ? ZoneCache : NegativeCache))
#else
      if (!(rr && (C = ZoneCache)))
#endif
	return ((void *)rr);

    /* Don't cache if TTL of this RR (or the parent SOA) is 0 */
    if ((rr && !rr->ttl) || (parent && !parent->ttl))
      return ((void *)rr);
  }
  C->misses++;

  /* If the cache is full, delete the least recently used node and add new node */
  if (C->count >= C->limit) {
    if (C->mruTail) {
      C->removed++;
      C->removed_secs += current_time - C->mruTail->insert_time;
      cache_free_node(C, C->mruTail->hash, C->mruTail);
    } else {
      return (type == DNS_QTYPE_SOA ? (void *)soa : (void *)rr);
    }
  }

  /* Add to cache */
  C->in++;
  n = ALLOCATE(sizeof(CNODE), CNODE);
  n->hash = hash;
  n->zone = zone;
  n->type = type;
  strncpy(n->name, name, sizeof(n->name)-1);
  n->namelen = namelen;
  n->insert_time = current_time;
  if (type == DNS_QTYPE_SOA) {
    if (C == ZoneCache)
      n->data = mydns_soa_dup(soa, 1);

    if (soa && (soa->ttl < (uint32_t)C->expire))
      n->expire = current_time + soa->ttl;
    else if (C->expire)
      n->expire = current_time + C->expire;
  } else {
    if (C == ZoneCache)
      n->data = mydns_rr_dup(rr, 1);

    if (rr && (rr->ttl < (uint32_t)C->expire))
      n->expire = current_time + rr->ttl;
    else if (C->expire)
      n->expire = current_time + C->expire;
  }
  n->next_node = C->nodes[hash];

  /* Add node to cache */
  C->nodes[hash] = n;
  C->count++;

  /* Add node to head of MRU list */
  mrulist_add(C, n);

  return (type == DNS_QTYPE_SOA ? (void *)soa : (void *)rr);
}
/*--- zone_cache_find() --------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_CACHE_FIND
	Attempt to find the reply data whole in the cache.
	Returns nonzero if found, 0 if not found.
	If found, fills in t->reply and t->replylen.
**************************************************************************************************/
int
reply_cache_find(TASK *t) {
  register uint32_t	hash = 0;
  register CNODE	*n = NULL;
  register void		*p = NULL;

  if (!ReplyCache || t->qdlen > DNS_MAXPACKETLEN_UDP)
    return (0);

#if STATUS_ENABLED
  /* Status requests aren't cached */
  if (t->qclass == DNS_CLASS_CHAOS)
    return (0);
#endif

  hash = cache_hash(ReplyCache, t->qtype, (void*)t->qd, t->qdlen);
  ReplyCache->questions++;

  /* Look at the appropriate node.  Descend list and find match. */
  for (n = ReplyCache->nodes[hash]; n; n = n->next_node) {
    if ((n->namelen == t->qdlen) && (n->type == t->qtype) && (n->protocol == t->protocol)) {
      if (!n->name)
	Errx(_("reply cache node %p at hash %u has NULL name"), n, hash);
      if (!memcmp(n->name, t->qd, t->qdlen)) {
	/* Is the node expired? */
	if (n->expire && (current_time > n->expire)) {
	  cache_free_node(ReplyCache, hash, n);
	  break;
	}

	/* Found in cache; move to head of usefulness list */
	mrulist_del(ReplyCache, n);
	mrulist_add(ReplyCache, n);

	/* Allocate space for reply data */
	t->replylen = n->datalen - sizeof(DNS_HEADER) - sizeof(task_error_t);
	t->reply = ALLOCATE(t->replylen, char[]);
	p = n->data;

	/* Copy DNS header */
	memcpy(&t->hdr, p, sizeof(DNS_HEADER));
	p = (void*)((unsigned char *)p + sizeof(DNS_HEADER));

	/* Copy reason */
	memcpy(&t->reason, p, sizeof(task_error_t));
	p = (void*)((unsigned char *)p + sizeof(task_error_t));

	/* Copy reply data */
	memcpy(t->reply, p, t->replylen);

	/* Set count of records in each section */
	p = t->reply + SIZE16 + SIZE16 + SIZE16;
	DNS_GET16(t->an.size, p);
	DNS_GET16(t->ns.size, p);
	DNS_GET16(t->ar.size, p);

	t->zone = n->zone;

	t->reply_from_cache = 1;
	ReplyCache->hits++;

	return (1);
      }
    }
  }
  ReplyCache->misses++;
  return (0);
}
/*--- reply_cache_find() ------------------------------------------------------------------------*/


/**************************************************************************************************
	ADD_REPLY_TO_CACHE
	Adds the current reply to the reply cache.
**************************************************************************************************/
void
add_reply_to_cache(TASK *t) {
  register uint32_t	hash = 0;
  register CNODE	*n = NULL;
  register void		*p = NULL;

  if (!ReplyCache || t->qdlen > DNS_MAXPACKETLEN_UDP || t->hdr.rcode == DNS_RCODE_SERVFAIL) {
    return;
  }

  /* Don't cache replies from recursive forwarder */
  if (t->forwarded)
    return;

#if STATUS_ENABLED
  /* Don't cache status requests */
  if (t->qclass == DNS_CLASS_CHAOS)
    return;
#endif

  /* Don't cache negative replies if recursive forwarding is enabled */
  if (forward_recursive && t->hdr.rcode != DNS_RCODE_NOERROR)
    return;

  hash = cache_hash(ReplyCache, t->qtype, (void*)t->qd, t->qdlen);

  /* Look at the appropriate node.  Descend list and find match. */
  for (n = ReplyCache->nodes[hash]; n; n = n->next_node) {
    if ((n->namelen == t->qdlen) && (n->type == t->qtype) && (n->protocol == t->protocol)) {
      if (!n->name)
	Errx(_("reply cache node %p at hash %u has NULL name"), n, hash);

      if (!memcmp(n->name, t->qd, t->qdlen)) {
	/* Is the node expired? */
	if (n->expire && (current_time > n->expire)) {
	  cache_free_node(ReplyCache, hash, n);
	  break;
	}

	/* Found in cache; move to head of usefulness list */
	mrulist_del(ReplyCache, n);
	mrulist_add(ReplyCache, n);
	return;
      }
    }
  }

  /* If the cache is full, delete the least recently used node and add new node */
  if (ReplyCache->count >= ReplyCache->limit) {
    if (ReplyCache->mruTail) {
      ReplyCache->removed++;
      ReplyCache->removed_secs += current_time - ReplyCache->mruTail->insert_time;
      cache_free_node(ReplyCache, ReplyCache->mruTail->hash, ReplyCache->mruTail);
    } else
      return;
  }

  /* Add to cache */
  ReplyCache->in++;

  n = ALLOCATE(sizeof(CNODE), CNODE);
  n->hash = hash;
  n->zone = t->zone;
  n->type = t->qtype;
  n->protocol = t->protocol;
  memcpy(n->name, t->qd, t->qdlen);
  n->namelen = t->qdlen;

  /* The data is the DNS_HEADER, the reason, then the reply */
  n->datalen = sizeof(DNS_HEADER) + sizeof(task_error_t) + t->replylen;
  n->data = ALLOCATE(n->datalen, char[]);
  p = n->data;

  /* Save DNS header */
  memcpy(p, &t->hdr, sizeof(DNS_HEADER));
  p = (void*)((unsigned char *)p + sizeof(DNS_HEADER));

  /* Save reason for error, if any */
  memcpy(p, &t->reason, sizeof(task_error_t));
  p = (void*)((unsigned char *)p + sizeof(task_error_t));

  /* Save reply data */
  memcpy(p, t->reply, t->replylen);

  n->insert_time = current_time;
  if (ReplyCache->expire)
    n->expire = current_time + ReplyCache->expire;
  n->next_node = ReplyCache->nodes[hash];

  /* Add node to cache */
  ReplyCache->nodes[hash] = n;
  ReplyCache->count++;

  /* Add node to head of MRU list */
  mrulist_add(ReplyCache, n);

}
/*--- add_reply_to_cache() ----------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
