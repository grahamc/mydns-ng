/**************************************************************************************************
	$Id: cache.h,v 1.53 2005/04/20 17:14:15 bboy Exp $

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

#ifndef _CACHE_H
#define _CACHE_H

/* The actual number of slots specified for `nodes' is the limit multiplied by this number.
	A higher value will yield greater speed (due to fewer collisions) but will use more memory. */
#define	CACHE_SLOT_MULTIPLIER	5

/* Set this to 1 to enable the negative cache */
#define	USE_NEGATIVE_CACHE	1

/* Hash types */
#define	ORIGINAL_HASH	0							/* Original hash */
#define	ADDITIVE_HASH	1							/* Additive hash */
#define	ROTATING_HASH	2							/* Rotating hash */
#define	FNV_HASH			3							/* FNV hash (http://www.isthe.com/chongo/tech/comp/fnv/) */

#ifndef	HASH_TYPE
#define	HASH_TYPE	ADDITIVE_HASH				/* Default hash type */
#endif

#define	FNV_32_INIT		((uint32_t)2166136261U)
#define	FNV_32_PRIME	((uint32_t)0x01000193)


typedef struct _cnode								/* A cache node */
{
	uint32_t		hash;						/* Hash value, for lookup when deleting nodes */
	uint32_t		zone;						/* Zone for record */

	dns_qtype_t		type;						/* Record type */
	int			protocol;					/* Protocol used (SOCK_DGRAM/SOCK_STREAM) */

	char			name[DNS_MAXNAMELEN + 1];			/* The name to look up */
	size_t			namelen;					/* strlen(name) */

	void			*data;						/* SOA or RR record or reply data (depending on `type') */
	size_t			datalen;					/* Length of data */

	time_t			insert_time;					/* Time record was inserted */
	time_t			expire;						/* Time after which this node should expire */

	struct _cnode *next_node;						/* Pointer to next node in bucket */
	struct _cnode *mruPrev, *mruNext;					/* Pointer to next/prev node in MRU/LRU list */
} CNODE;


typedef struct _cache								/* A cache */
{
	char			name[20];							/* Name of this cache */

	size_t		limit;								/* Max number of nodes allowed (via `cache-size') */
	size_t		count;								/* Number of used nodes in the cache */

	size_t		size;									/* Number of bytes used by this cache */
															/* (not updated unless cache_size_update() is called) */

	time_t		expire;								/* Expiration seconds for cached items */

	uint32_t		questions;							/* Total number of questions */
	uint32_t		hits, misses;						/* Total number of hits/misses for the cache */
	uint32_t		in, out;								/* Cache entries in and out */
	uint32_t		expired, removed;					/* Cache entries expired and removed */

	time_t		removed_secs;						/* Total lifetime seconds of removed entries */

	uint32_t		slots;								/* Number of slots in `nodes' array */
#if (HASH_TYPE == ROTATING_HASH)
	uint32_t		mask;									/* Hash mask */
#endif                                       
#if (HASH_TYPE == FNV_HASH)
	uint32_t		bits;									/* Bit length of hash table */
#endif

	CNODE			**nodes;								/* Array of nodes */
                                             
	CNODE			*mruHead, *mruTail;				/* Most/Least recently used (MRU/LRU) nodes (head/tail) */
} CACHE;


extern CACHE *ZoneCache;							/* Zone cache */
extern CACHE *ReplyCache;							/* Reply cache */

#if USE_NEGATIVE_CACHE
extern CACHE *NegativeCache;						/* Negative zone cache */
#endif

extern void cache_status(CACHE *);
extern void cache_init(void), cache_empty(CACHE *), cache_cleanup(CACHE *);
extern void cache_purge_zone(CACHE *, uint32_t);
extern void *zone_cache_find(TASK *, uint32_t, char *, dns_qtype_t, const char *, size_t, int *, MYDNS_SOA *);

extern int  reply_cache_find(TASK *);
extern void add_reply_to_cache(TASK *);


#endif /* _CACHE_H */

/* vi:set ts=3: */
