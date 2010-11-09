/**************************************************************************************************
	$Id: rr.c,v 1.65 2005/04/29 16:10:27 bboy Exp $

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

#include "mydns.h"

#define __MYDNS_RR_NAME(__rrp)			((__rrp)->_name)
#define __MYDNS_RR_DATA(__rrp)			((__rrp)->_data)
#define __MYDNS_RR_DATA_LENGTH(__rrp)		((__rrp)->_data.len)
#define __MYDNS_RR_DATA_VALUE(__rrp)		((__rrp)->_data.value)
#define __MYDNS_RR_SRV_WEIGHT(__rrp)		((__rrp)->recData.srv.weight)
#define __MYDNS_RR_SRV_PORT(__rrp)		((__rrp)->recData.srv.port)
#define __MYDNS_RR_RP_TXT(__rrp)		((__rrp)->recData._rp_txt)
#define __MYDNS_RR_NAPTR_ORDER(__rrp)		((__rrp)->recData.naptr.order)
#define __MYDNS_RR_NAPTR_PREF(__rrp)		((__rrp)->recData.naptr.pref)
#define __MYDNS_RR_NAPTR_FLAGS(__rrp)		((__rrp)->recData.naptr.flags)
#define __MYDNS_RR_NAPTR_SERVICE(__rrp)		((__rrp)->recData.naptr._service)
#define __MYDNS_RR_NAPTR_REGEX(__rrp)		((__rrp)->recData.naptr._regex)
#define __MYDNS_RR_NAPTR_REPLACEMENT(__rrp)	((__rrp)->recData.naptr._replacement)

char *mydns_rr_table_name = NULL;
char *mydns_rr_where_clause = NULL;

size_t mydns_rr_data_length = DNS_DATALEN;

/* Optional columns */
int mydns_rr_extended_data = 0;
int mydns_rr_use_active = 0;
int mydns_rr_use_stamp = 0;
int mydns_rr_use_serial = 0;

char *mydns_rr_active_types[] = { (char*)"Y", (char*)"N", (char*)"D" };

/* Make this nonzero to enable debugging within this source file */
#define	DEBUG_LIB_RR	1

#if DEBUG_ENABLED
void *
__mydns_rr_assert_pointer(void *ptr, const char *fieldname, const char *filename, int linenumber) {
#if DEBUG_ENABLED && DEBUG_LIB_RR
  DebugX("lib-rr", 1, _("mydns_rr_assert_pointer() called for field=%s from %s:%d"),
	 fieldname, filename, linenumber);
#endif
  if (ptr != NULL) return ptr;
  DebugX("lib-rr", 1, _("%s Pointer is NULL at %s:%d"),
	 fieldname, filename, linenumber);
  abort();
  return ptr;
}
#endif

void
mydns_rr_get_active_types(SQL *sqlConn) {
  SQL_RES	*res;
  SQL_ROW	row;
  int 		querylen;
  char		*query;

  char		*YES = (char*)"Y";
  char		*NO = (char*)"N";
  char		*DELETED = (char*)"D";

  querylen = sql_build_query(&query, "SELECT DISTINCT(active) FROM %s", mydns_rr_table_name);

  if (!(res = sql_query(sqlConn, query, querylen))) return;

  RELEASE(query);

  while ((row = sql_getrow(res, NULL))) {
    char *VAL = row[0];
    if (   !strcasecmp(VAL, "yes")
	   || !strcasecmp(VAL, "y")
	   || !strcasecmp(VAL, "true")
	   || !strcasecmp(VAL, "t")
	   || !strcasecmp(VAL, "active")
	   || !strcasecmp(VAL, "a")
	   || !strcasecmp(VAL, "on")
	   || !strcmp(VAL, "1") ) { YES = STRDUP(VAL); continue; }
    if (   !strcasecmp(VAL, "no")
	   || !strcasecmp(VAL, "n")
	   || !strcasecmp(VAL, "false")
	   || !strcasecmp(VAL, "f")
	   || !strcasecmp(VAL, "inactive")
	   || !strcasecmp(VAL, "i")
	   || !strcasecmp(VAL, "off")
	   || !strcmp(VAL, "0") ) { NO = STRDUP(VAL); continue; }
    if (   !strcasecmp(VAL, "d")
	   || !strcasecmp(VAL, "deleted")
	   || !strcmp(VAL, "2") ) { DELETED = STRDUP(VAL); continue; }
  }

  sql_free(res);

  mydns_rr_active_types[0] = YES;
  mydns_rr_active_types[1] = NO;
  mydns_rr_active_types[2] = DELETED;
}

/**************************************************************************************************
	MYDNS_RR_COUNT
	Returns the number of records in the rr table.
**************************************************************************************************/
long
mydns_rr_count(SQL *sqlConn) {
  return sql_count(sqlConn, "SELECT COUNT(*) FROM %s", mydns_rr_table_name);
}
/*--- mydns_rr_count() --------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_SET_RR_TABLE_NAME
**************************************************************************************************/
void
mydns_set_rr_table_name(const char *name) {
  RELEASE(mydns_rr_table_name);
  if (!name)
    mydns_rr_table_name = STRDUP(MYDNS_RR_TABLE);
  else
    mydns_rr_table_name = STRDUP(name);
}
/*--- mydns_set_rr_table_name() -----------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_SET_RR_WHERE_CLAUSE
**************************************************************************************************/
void
mydns_set_rr_where_clause(const char *where) {
  if (where && strlen(where)) {
    mydns_rr_where_clause = STRDUP(where);
  }
}
/*--- mydns_set_rr_where_clause() ---------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_RR_GET_TYPE
**************************************************************************************************/
inline dns_qtype_t
mydns_rr_get_type(char *type) {
  register char *c;

  for (c = type; *c; c++)
    *c = toupper(*c);

  switch (type[0]) {
  case 'A':
    if (!type[1])
      return DNS_QTYPE_A;

    if (type[1] == 'A' && type[2] == 'A' && type[3] == 'A' && !type[4])
      return DNS_QTYPE_AAAA;

#if ALIAS_ENABLED
    if (type[1] == 'L' && type[2] == 'I' && type[3] == 'A' && type[4] == 'S' && !type[5])
      return DNS_QTYPE_ALIAS;
#endif
    break;

  case 'C':
    if (type[1] == 'N' && type[2] == 'A' && type[3] == 'M' && type[4] == 'E' && !type[5])
      return DNS_QTYPE_CNAME;
    break;

  case 'H':
    if (type[1] == 'I' && type[2] == 'N' && type[3] == 'F' && type[4] == 'O' && !type[5])
      return DNS_QTYPE_HINFO;
    break;

  case 'M':
    if (type[1] == 'X' && !type[2])
      return DNS_QTYPE_MX;
    break;

  case 'N':
    if (type[1] == 'S' && !type[2])
      return DNS_QTYPE_NS;
    if (type[1] == 'A' && type[2] == 'P' && type[3] == 'T' && type[4] == 'R' && !type[5])
      return DNS_QTYPE_NAPTR;
    break;

  case 'T':
    if (type[1] == 'X' && type[2] == 'T' && !type[3])
      return DNS_QTYPE_TXT;
    break;

  case 'P':
    if (type[1] == 'T' && type[2] == 'R' && !type[3])
      return DNS_QTYPE_PTR;
    break;

  case 'R':
    if (type[1] == 'P' && !type[2])
      return DNS_QTYPE_RP;
    break;

  case 'S':
    if (type[1] == 'R' && type[2] == 'V' && !type[3])
      return DNS_QTYPE_SRV;
    break;
  }
  return 0;
}
/*--- mydns_rr_get_type() -----------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_RR_PARSE_RP
	RP contains two names in 'data' -- the mbox and the txt.
	NUL-terminate mbox and fill 'rp_txt' with the txt part of the record.
**************************************************************************************************/
static inline void
mydns_rr_parse_rp(const char *origin, MYDNS_RR *rr) {
  char *c;

  /* If no space, set txt to '.' */
  if (!(c = strchr(__MYDNS_RR_DATA_VALUE(rr), ' '))) {
    __MYDNS_RR_RP_TXT(rr) = STRDUP(".");
  } else {
    int namelen = strlen(&c[1]);
    if (LASTCHAR(&c[1]) != '.') {
      namelen += strlen(origin) + 1;
    }
    __MYDNS_RR_RP_TXT(rr) = ALLOCATE_N(namelen + 1, sizeof(char), char[]);
    strncpy(__MYDNS_RR_RP_TXT(rr), &c[1], namelen);
    if (LASTCHAR(__MYDNS_RR_RP_TXT(rr)) != '.') {
      strncat(__MYDNS_RR_RP_TXT(rr), ".", namelen);
      strncat(__MYDNS_RR_RP_TXT(rr), origin, namelen);
    }
    *c = '\0';
    __MYDNS_RR_DATA_VALUE(rr) = REALLOCATE(__MYDNS_RR_DATA_VALUE(rr),
					   strlen(__MYDNS_RR_DATA_VALUE(rr))+1,
					   char[]);
    __MYDNS_RR_DATA_LENGTH(rr) = strlen(__MYDNS_RR_DATA_VALUE(rr));
  }
}
/*--- mydns_rr_parse_rp() -----------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_RR_PARSE_SRV
	SRV records contain two unsigned 16-bit integers in the "data" field before the target,
	'srv_weight' and 'srv_port' - parse them and make "data" contain only the target.  Also, make
	sure 'aux' fits into 16 bits, clamping values above 65535.
**************************************************************************************************/
static inline void
mydns_rr_parse_srv(const char *origin, MYDNS_RR *rr) {
  char *weight, *port, *target;

  /* Clamp 'aux' if necessary */
  if (rr->aux > 65535)
    rr->aux = 65535;

  /* Parse weight (into srv_weight), port (into srv_port), and target */
  target = __MYDNS_RR_DATA_VALUE(rr);
  if ((weight = strsep(&target, " \t"))) {
    __MYDNS_RR_SRV_WEIGHT(rr) = atoi(weight);
    if ((port = strsep(&target, " \t")))
      __MYDNS_RR_SRV_PORT(rr) = atoi(port);

    /* Strip the leading data off and just hold target */
    memmove(__MYDNS_RR_DATA_VALUE(rr), target, strlen(target)+1);
    __MYDNS_RR_DATA_LENGTH(rr) = strlen(__MYDNS_RR_DATA_VALUE(rr));
    __MYDNS_RR_DATA_VALUE(rr) = REALLOCATE(__MYDNS_RR_DATA_VALUE(rr),
					   __MYDNS_RR_DATA_LENGTH(rr) + 1,
					   char[]);
  }
}
/*--- mydns_rr_parse_srv() ----------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_RR_PARSE_NAPTR
	Returns 0 on success, -1 on error.
**************************************************************************************************/
static inline int
mydns_rr_parse_naptr(const char *origin, MYDNS_RR *rr) {
  char 		*int_tmp, *p, *data_copy;

  data_copy = STRNDUP(__MYDNS_RR_DATA_VALUE(rr), __MYDNS_RR_DATA_LENGTH(rr));
  p = data_copy;

  if (!strsep_quotes2(&p, &int_tmp))
    return (-1);
  __MYDNS_RR_NAPTR_ORDER(rr) = atoi(int_tmp);
  RELEASE(int_tmp);

  if (!strsep_quotes2(&p, &int_tmp))
    return (-1);
  __MYDNS_RR_NAPTR_PREF(rr) = atoi(int_tmp);
  RELEASE(int_tmp);

  if (!strsep_quotes(&p, __MYDNS_RR_NAPTR_FLAGS(rr), sizeof(__MYDNS_RR_NAPTR_FLAGS(rr))))
    return (-1);

  if (!strsep_quotes2(&p, &__MYDNS_RR_NAPTR_SERVICE(rr)))
    return (-1);

  if (!strsep_quotes2(&p, &__MYDNS_RR_NAPTR_REGEX(rr)))
    return (-1);

  if (!strsep_quotes2(&p, &__MYDNS_RR_NAPTR_REPLACEMENT(rr)))
    return (-1);

  //__MYDNS_RR_DATA_LENGTH(rr) = 0;
//  RELEASE(__MYDNS_RR_DATA_VALUE(rr));

  RELEASE(data_copy);
  return 0;
}
/*--- mydns_rr_parse_naptr() --------------------------------------------------------------------*/

static inline int
mydns_rr_parse_txt(const char *origin, MYDNS_RR *rr) {
  int datalen = __MYDNS_RR_DATA_LENGTH(rr);
  char *data = __MYDNS_RR_DATA_VALUE(rr);

  if (datalen > DNS_MAXTXTLEN) return (-1);

  while (datalen > 0) {
    size_t elemlen = strlen(data);
    if (elemlen > DNS_MAXTXTELEMLEN) return (-1);
    data = &data[elemlen+1];
    datalen -= elemlen + 1;
  }
  
  return 0;
}

static char *
__mydns_rr_append(char *s1, char *s2) {
  int s1len = strlen(s1);
  int s2len = strlen(s2);
  int newlen = s1len;
  char *name;
  if (s1len) newlen += 1;
  newlen += s2len;

  name = ALLOCATE(newlen+1, char[]);
  if (s1len) { strncpy(name, s1, s1len); name[s1len] = '.'; s1len += 1; }
  strncpy(&name[s1len], s2, s2len);
  name[newlen] = '\0';
  return name;
}

char *
mydns_rr_append_origin(char *str, char *origin) {
  char *res = ((!*str || LASTCHAR(str) != '.')
	       ?__mydns_rr_append(str, origin)
	       :str);
  return res;
}

void
mydns_rr_name_append_origin(MYDNS_RR *rr, char *origin) {
  char *res = mydns_rr_append_origin(__MYDNS_RR_NAME(rr), origin);
  if (__MYDNS_RR_NAME(rr) != res) RELEASE(__MYDNS_RR_NAME(rr));
  __MYDNS_RR_NAME(rr) = res;
}
      
void
mydns_rr_data_append_origin(MYDNS_RR *rr, char *origin) {
  char *res = mydns_rr_append_origin(__MYDNS_RR_DATA_VALUE(rr), origin);
  if (__MYDNS_RR_DATA_VALUE(rr) != res) RELEASE(__MYDNS_RR_DATA_VALUE(rr));
  __MYDNS_RR_DATA_VALUE(rr) = res;
  __MYDNS_RR_DATA_LENGTH(rr) = strlen(__MYDNS_RR_DATA_VALUE(rr));
}
      
/**************************************************************************************************
	_MYDNS_RR_FREE
	Frees the pointed-to structure.	Don't call this function directly, call the macro.
**************************************************************************************************/
void
_mydns_rr_free(MYDNS_RR *first) {
  register MYDNS_RR *p, *tmp;

  for (p = first; p; p = tmp) {
    tmp = p->next;
    RELEASE(p->stamp);
    RELEASE(__MYDNS_RR_NAME(p));
    RELEASE(__MYDNS_RR_DATA_VALUE(p));
    switch (p->type) {
    case DNS_QTYPE_NAPTR:
      RELEASE(__MYDNS_RR_NAPTR_SERVICE(p));
      RELEASE(__MYDNS_RR_NAPTR_REGEX(p));
      RELEASE(__MYDNS_RR_NAPTR_REPLACEMENT(p));
      break;
    case DNS_QTYPE_RP:
      RELEASE(__MYDNS_RR_RP_TXT(p));
      break;
    default:
      break;
    }
    RELEASE(p);
  }
}
/*--- _mydns_rr_free() --------------------------------------------------------------------------*/

MYDNS_RR *
mydns_rr_build(uint32_t id,
	       uint32_t zone,
	       dns_qtype_t type,
	       dns_class_t class,
	       uint32_t aux,
	       uint32_t ttl,
	       char *active,
#if USE_PGSQL
	       timestamp *stamp,
#else
	       MYSQL_TIME *stamp,
#endif
	       uint32_t serial,
	       char *name,
	       char *data,
	       uint16_t	datalen,
	       const char *origin) {
  MYDNS_RR	*rr = NULL;
  uint32_t	namelen;

#if DEBUG_ENABLED && DEBUG_LIB_RR
  DebugX("lib-rr", 1, _("mydns_rr_build(): called for id=%d, zone=%d, type=%d, class=%d, aux=%d, "
			"ttl=%d, active='%s', stamp=%p, serial=%d, name='%s', data=%p, datalen=%d, origin='%s'"),
	 id, zone, type, class, aux, ttl, active, stamp, serial,
	 (name)?name:_("<NULL>"), data, datalen, origin);
#endif

  if ((namelen = (name)?strlen(name):0) > DNS_MAXNAMELEN) {
    /* Name exceeds permissable length - should report error */
    goto PARSEFAILED;
  }

  rr = (MYDNS_RR *)ALLOCATE(sizeof(MYDNS_RR), MYDNS_RR);
  memset(rr, '\0', sizeof(MYDNS_RR));
  rr->next = NULL;

  rr->id = id;
  rr->zone = zone;

  __MYDNS_RR_NAME(rr) = ALLOCATE(namelen+1, char[]);
  memset(__MYDNS_RR_NAME(rr), 0, namelen+1);
  if (name) strncpy(__MYDNS_RR_NAME(rr), name, namelen);

  /* Should store length and buffer rather than handle as a string */
  __MYDNS_RR_DATA_LENGTH(rr) = datalen;
  __MYDNS_RR_DATA_VALUE(rr) = ALLOCATE(datalen+1, char[]);
  memcpy(__MYDNS_RR_DATA_VALUE(rr), data, datalen);

  rr->class = class;
  rr->aux = aux;
  rr->ttl = ttl;
  rr->type = type;
#if ALIAS_ENABLED
  if (rr->type == DNS_QTYPE_ALIAS) {
    rr->type = DNS_QTYPE_A;
    rr->alias = 1;
  } else
    rr->alias = 0;
#endif

  /* Find a constant value so we do not have to allocate or free this one */
  if (active) {
    int i;
    for (i = 0; i < 3; i++) {
      if (!strcasecmp(mydns_rr_active_types[i], active)) { active = mydns_rr_active_types[i]; break; }
    }
  }
  rr->active = active;
  rr->stamp = stamp;
  rr->serial = serial;

  switch (rr->type) {

  case DNS_QTYPE_TXT:
    if (mydns_rr_parse_txt(origin, rr) < 0) {
      goto PARSEFAILED;
    }
    break;

  case DNS_QTYPE_NAPTR:
    /* Populate special fields for NAPTR records */
    if (mydns_rr_parse_naptr(origin, rr) < 0) {
      goto PARSEFAILED;
    }
    break;

  case DNS_QTYPE_RP:
    /* Populate special fields for RP records */
    mydns_rr_parse_rp(origin, rr);
    goto DOORIGIN;

  case DNS_QTYPE_SRV:
    mydns_rr_parse_srv(origin, rr);
    goto DOORIGIN;

  DOORIGIN:

  case DNS_QTYPE_CNAME:
  case DNS_QTYPE_MX:
  case DNS_QTYPE_NS:

    /* Append origin to data if it's not there for these types: */
    if (origin) {
      datalen = __MYDNS_RR_DATA_LENGTH(rr);
#ifdef DN_COLUMN_NAMES
      datalen += 1;
      __MYDNS_RR_DATA_LENGTH(rr) = datalen;
      __MYDNS_RR_DATA_VALUE(rr) = REALLOCATE(__MYDNS_RR_DATA_VALUE(rr), datalen+1, char[]);
      /* Just append dot for DN */
      ((char*)__MYDNS_RR_DATA_VALUE(rr))[datalen-1] = '.';
#else
      if (datalen && ((char*)__MYDNS_RR_DATA_VALUE(rr))[datalen-1] != '.') {
	datalen = datalen + 1 + strlen(origin);
	__MYDNS_RR_DATA_VALUE(rr) = REALLOCATE(__MYDNS_RR_DATA_VALUE(rr), datalen+1, char[]);
	((char*)__MYDNS_RR_DATA_VALUE(rr))[__MYDNS_RR_DATA_LENGTH(rr)] = '.';
	memcpy(&((char*)__MYDNS_RR_DATA_VALUE(rr))[__MYDNS_RR_DATA_LENGTH(rr)+1], origin, strlen(origin));
	__MYDNS_RR_DATA_LENGTH(rr) = datalen;
      }
#endif
      ((char*)__MYDNS_RR_DATA_VALUE(rr))[__MYDNS_RR_DATA_LENGTH(rr)] = '\0';
    }
    break;
  default:
    break;
  }

#if DEBUG_ENABLED && DEBUG_LIB_RR
  DebugX("lib-rr", 1, _("mydns_rr_build(): returning result=%p"), rr);
#endif
  return (rr);

 PARSEFAILED:
  mydns_rr_free(rr);
  return (NULL);
}

/**************************************************************************************************
	MYDNS_RR_PARSE
	Given the SQL results with RR data, populates and returns a matching MYDNS_RR structure.
	Returns NULL on error.
**************************************************************************************************/
inline MYDNS_RR *
mydns_rr_parse(SQL_ROW row, unsigned long *lengths, const char *origin) {
  dns_qtype_t	type;
  char		*active = NULL;
#if USE_PGSQL
  timestamp	*stamp = NULL;
#else
  MYSQL_TIME	*stamp = NULL;
#endif
  uint32_t	serial = 0;
  int		ridx = MYDNS_RR_NUMFIELDS;
  char		*data;
  uint16_t	datalen;
  MYDNS_RR	*rr;

#if DEBUG_ENABLED && DEBUG_LIB_RR
  DebugX("lib-rr", 1, _("mydns_rr_parse(): called for origin %s"), origin);
#endif

/* #60 */
//if (!(type = mydns_rr_get_type(row[6]))) {
  if (!row[6] || !(type = mydns_rr_get_type(row[6]))) {
    /* Ignore unknown RR type(s) */
    return (NULL);
  }

  data = row[3];
  datalen = lengths[3];
  if (mydns_rr_extended_data) {
    if (lengths[ridx]) {
      char *newdata = ALLOCATE(datalen + lengths[ridx], char[]);
      memcpy(newdata, data, datalen);
      memcpy(&newdata[datalen], row[ridx], lengths[ridx]);
      datalen += lengths[ridx];
      data = newdata;
    }
    ridx++;
  }

  /* Copy storage? */
  if (mydns_rr_use_active) active = row[ridx++];
  if (mydns_rr_use_stamp) {
#if USE_PGSQL
    /* Copy storage? */
    stamp = row[ridx++];
#else
    stamp = (MYSQL_TIME*)ALLOCATE(sizeof(MYSQL_TIME), MYSQL_TIME);
    memcpy(stamp, row[ridx++], sizeof(MYSQL_TIME));
#endif
  }
  if (mydns_rr_use_serial && row[ridx]) {
    serial = atou(row[ridx++]);
  }

  rr = mydns_rr_build(atou(row[0]),
		      atou(row[1]),
		      type,
		      DNS_CLASS_IN,
		      atou(row[4]),
		      atou(row[5]),
		      active,
		      stamp,
		      serial,
		      row[2],
		      data,
		      datalen,
		      origin);

  if (mydns_rr_extended_data && lengths[MYDNS_RR_NUMFIELDS]) RELEASE(data);

  return (rr);
}
/*--- mydns_rr_parse() --------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_RR_DUP
	Make and return a copy of a MYDNS_RR record.  If 'recurse' is specified, copies all records
	in the RRset.
**************************************************************************************************/
MYDNS_RR *
mydns_rr_dup(MYDNS_RR *start, int recurse) {
  register MYDNS_RR *first = NULL, *last = NULL, *rr, *s, *tmp;

  for (s = start; s; s = tmp) {
    tmp = s->next;

    rr = (MYDNS_RR *)ALLOCATE(sizeof(MYDNS_RR), MYDNS_RR);

    memset(rr, '\0', sizeof(MYDNS_RR));
    rr->id = s->id;
    rr->zone = s->zone;
    __MYDNS_RR_NAME(rr) = STRDUP(__MYDNS_RR_NAME(s));
    rr->type = s->type;
    rr->class = s->class;
    __MYDNS_RR_DATA_LENGTH(rr) = __MYDNS_RR_DATA_LENGTH(s);
    __MYDNS_RR_DATA_VALUE(rr) = ALLOCATE(__MYDNS_RR_DATA_LENGTH(s)+1, char[]);
    memcpy(__MYDNS_RR_DATA_VALUE(rr), __MYDNS_RR_DATA_VALUE(s), __MYDNS_RR_DATA_LENGTH(s));
    ((char*)__MYDNS_RR_DATA_VALUE(rr))[__MYDNS_RR_DATA_LENGTH(rr)] = '\0';
    rr->aux = s->aux;
    rr->ttl = s->ttl;
#if ALIAS_ENABLED
    rr->alias = s->alias;
#endif

    rr->active = s->active;
    if (s->stamp) {
#if USE_PGSQL
      rr->stamp = s->stamp;
#else
      rr->stamp = (MYSQL_TIME*)ALLOCATE(sizeof(MYSQL_TIME), MYSQL_TIME);
      memcpy(rr->stamp, s->stamp, sizeof(MYSQL_TIME));
#endif
    } else
      rr->stamp = NULL;
    rr->serial = s->serial;

    switch (rr->type) {
    case DNS_QTYPE_SRV:
      __MYDNS_RR_SRV_WEIGHT(rr) = __MYDNS_RR_SRV_WEIGHT(s);
      __MYDNS_RR_SRV_PORT(rr) = __MYDNS_RR_SRV_PORT(s);
      break;

    case DNS_QTYPE_RP:
      /* Copy rp_txt only for RP records */
      __MYDNS_RR_RP_TXT(rr) = STRDUP(__MYDNS_RR_RP_TXT(s));
      break;

    case DNS_QTYPE_NAPTR:
      /* Copy naptr fields only for NAPTR records */
      __MYDNS_RR_NAPTR_ORDER(rr) = __MYDNS_RR_NAPTR_ORDER(s);
      __MYDNS_RR_NAPTR_PREF(rr) = __MYDNS_RR_NAPTR_PREF(s);
      memcpy(__MYDNS_RR_NAPTR_FLAGS(rr), __MYDNS_RR_NAPTR_FLAGS(s), sizeof(__MYDNS_RR_NAPTR_FLAGS(rr)));
      __MYDNS_RR_NAPTR_SERVICE(rr) = STRDUP(__MYDNS_RR_NAPTR_SERVICE(s));
      __MYDNS_RR_NAPTR_REGEX(rr) = STRDUP(__MYDNS_RR_NAPTR_REGEX(s));
      __MYDNS_RR_NAPTR_REPLACEMENT(rr) = STRDUP(__MYDNS_RR_NAPTR_REPLACEMENT(s));
      break;

    default:
      break;
    }

    rr->next = NULL;
    if (recurse) {
      if (!first) first = rr;
      if (last) last->next = rr;
      last = rr;
    } else
      return (rr);
  }
  return (first);
}
/*--- mydns_rr_dup() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_RR_SIZE
**************************************************************************************************/
inline size_t
mydns_rr_size(MYDNS_RR *first) {
  register MYDNS_RR *p;
  register size_t size = 0;

  for (p = first; p; p = p->next) {
    size += sizeof(MYDNS_RR)
      + (strlen(__MYDNS_RR_NAME(p)) + 1)
      + (__MYDNS_RR_DATA_LENGTH(p) + 1);
#if USE_PGSQL
#else
    size += sizeof(MYSQL_TIME);
#endif
    switch (p->type) {
    case DNS_QTYPE_NAPTR:
      size += strlen(__MYDNS_RR_NAPTR_SERVICE(p)) + 1;
      size += strlen(__MYDNS_RR_NAPTR_REGEX(p)) + 1;
      size += strlen(__MYDNS_RR_NAPTR_REPLACEMENT(p)) + 1;
      break;

    case DNS_QTYPE_RP:
      size += strlen(__MYDNS_RR_RP_TXT(p)) + 1;
      break;

    default:
      break;
    }
  }    

  return (size);
}
/*--- mydns_rr_size() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_RR_LOAD
	Returns 0 on success or nonzero if an error occurred.
	If "name" is NULL, all resource records for the zone will be loaded.
**************************************************************************************************/
char *
mydns_rr_columns() {
  char		*columns = NULL;
  size_t	columnslen = 0;

  columnslen = sql_build_query(&columns, MYDNS_RR_FIELDS"%s%s%s%s",
			       /* Optional columns */
			       (mydns_rr_extended_data ? ",edata" : ""),
			       (mydns_rr_use_active ? ",active" : ""),
			       (mydns_rr_use_stamp  ? ",stamp"  : ""),
			       (mydns_rr_use_serial ? ",serial" : ""));
  return columns;
}

char *
mydns_rr_prepare_query(uint32_t zone, dns_qtype_t type, const char *name, const char *origin,
		       const char *active, const char *columns, const char *filter) {
  size_t	querylen;
  char		*query = NULL;
  char		*namequery = NULL;
  const char	*wheretype;
  const char	*cp;
#ifdef DN_COLUMN_NAMES
  int		originlen = origin ? strlen(origin) : 0;
  int		namelen = name ? strlen(name) : 0;
#endif

#if DEBUG_ENABLED && DEBUG_LIB_RR
  DebugX("lib-rr", 1, _("mydns_rr_prepare_query(zone=%u, type='%s', name='%s', origin='%s')"),
	 zone, mydns_qtype_str(type), name ?: _("NULL"), origin ?: _("NULL"));
#endif

  /* Get the type='XX' part of the WHERE clause */
  switch (type)	{
#if ALIAS_ENABLED
  case DNS_QTYPE_A:		wheretype = " AND (type='A' OR type='ALIAS')"; break;
#else
  case DNS_QTYPE_A:		wheretype = " AND type='A'"; break;
#endif
  case DNS_QTYPE_AAAA:		wheretype = " AND type='AAAA'"; break;
  case DNS_QTYPE_CNAME:	        wheretype = " AND type='CNAME'"; break;
  case DNS_QTYPE_HINFO:	        wheretype = " AND type='HINFO'"; break;
  case DNS_QTYPE_MX:		wheretype = " AND type='MX'"; break;
  case DNS_QTYPE_NAPTR:	        wheretype = " AND type='NAPTR'"; break;
  case DNS_QTYPE_NS:		wheretype = " AND type='NS'"; break;
  case DNS_QTYPE_PTR:		wheretype = " AND type='PTR'"; break;
  case DNS_QTYPE_SOA:		wheretype = " AND type='SOA'"; break;
  case DNS_QTYPE_SRV:		wheretype = " AND type='SRV'"; break;
  case DNS_QTYPE_TXT:		wheretype = " AND type='TXT'"; break;
  case DNS_QTYPE_ANY:		wheretype = ""; break;
  default:
    errno = EINVAL;
    return (NULL);
  }

  /* Make sure 'name' and 'origin' (if present) are valid */
  if (name) {
    for (cp = name; *cp; cp++)
      if (SQL_BADCHAR(*cp))
	return (NULL);
  }
  if (origin) {
    for (cp = origin; *cp; cp++)
      if (SQL_BADCHAR(*cp))
	return (NULL);
  }

#ifdef DN_COLUMN_NAMES
  /* Remove dot from origin and name for DN */
  if (originlen && origin[originlen - 1] == '.')
    origin[originlen-1] = '\0';
  else
    originlen = 0;

  if (name) {
    if (namelen && name[namelen - 1] == '.')
      name[namelen-1] = '\0';
    else
      namelen = 0;
  }
#endif

  /* Construct query */
  if (name) {
    if (origin) {
      if (!name[0])
	sql_build_query(&namequery, "(name='' OR name='%s')", origin);
      else {
#ifdef DN_COLUMN_NAMES
	sql_build_query(&namequery, "name='%s'", name);
#else
	sql_build_query(&namequery, "(name='%s' OR name='%s.%s')", name, name, origin);
#endif
      }
    }
    else
      sql_build_query(&namequery, "name='%s'", name);
  }

#ifdef DN_COLUMN_NAMES
  if (originlen)
    origin[originlen - 1] = '.';				/* Re-add dot to origin for DN */

  if (name) {
    if (namelen)
      name[namelen - 1] = '.';
  }
#endif

  querylen = sql_build_query(&query, "SELECT %s FROM %s WHERE "
#ifdef DN_COLUMN_NAMES
			     "zone_id=%u%s"
#else
			     "zone=%u%s"
#endif
			     "%s%s"
			     "%s%s%s"
			     "%s%s"
			     "%s%s"
			     "%s",

			     columns,

			     /* Fixed data */
			     mydns_rr_table_name,
			     zone, wheretype,

			     /* Name based query */
			     (namequery)? " AND " : "",
			     (namequery)? namequery : "",

			     /* Optional check for active value */
			     (mydns_rr_use_active)? " AND active='" : "",
			     (mydns_rr_use_active)? active : "",
			     (mydns_rr_use_active)? "'" : "",

			     /* Optional where clause for rr table */
			     (mydns_rr_where_clause)? " AND " : "",
			     (mydns_rr_where_clause)? mydns_rr_where_clause : "",

			     /* Apply additional filter if requested */
			     (filter)? " AND " : "",
			     (filter)? filter : "",

			     /* Optional sorting */
			     (mydns_rr_use_stamp)? " ORDER BY stamp DESC" : "");

  RELEASE(namequery);

  return (query);
}
			 
static int __mydns_rr_do_load(SQL *sqlConn, MYDNS_RR **rptr, const char *query, const char *origin) {
  MYDNS_RR	*first = NULL, *last = NULL;
  char		*cp;
  SQL_RES	*res;
  SQL_ROW	row;
  unsigned long *lengths;


#if DEBUG_ENABLED && DEBUG_LIB_RR
  DebugX("lib-rr", 1, _("mydns_rr_do_load(query='%s', origin='%s')"), query, origin ? origin : _("NULL"));
#endif

  if (rptr) *rptr = NULL;

  /* Verify args */
  if (!sqlConn || !rptr || !query) {
    errno = EINVAL;
    return (-1);
  }

  /* Submit query */
  if (!(res = sql_query(sqlConn, query, strlen(query))))
    return (-1);

#if DEBUG_ENABLED && DEBUG_LIB_RR
  {
    int numresults = sql_num_rows(res);

    DebugX("lib-rr", 1, _("RR query: %d row%s: %s"), numresults, S(numresults), query);
  }
#endif

  RELEASE(query);

  /* Add results to list */
  while ((row = sql_getrow(res, &lengths))) {
    MYDNS_RR *new;

    if (!(new = mydns_rr_parse(row, lengths, origin)))
      continue;

    /* Always trim origin from name (XXX: Why? When did I add this?) */
    /* Apparently removing this code breaks RRs where the name IS the origin */
    /* But trim only where the name is exactly the origin */
    if (origin && (cp = strstr(__MYDNS_RR_NAME(new), origin)) && !(cp - __MYDNS_RR_NAME(new)))
      *cp = '\0';

    if (!first) first = new;
    if (last) last->next = new;
    last = new;
  }

  *rptr = first;
  sql_free(res);
  return (0);
}

static int
__mydns_rr_count(SQL *sqlConn, uint32_t zone,
		 dns_qtype_t type,
		 const char *name, const char *origin, const char *active, const char *filter) {
  char		*query = NULL;
  int		result;

  SQL_RES	*res;
  SQL_ROW	row;

  query = mydns_rr_prepare_query(zone, type, name, origin, active, (char*)"COUNT(*)", filter);

  if (!query || !(res = sql_query(sqlConn, query, strlen(query)))) {
    WarnSQL(sqlConn, _("error processing count with filter %s"), filter);
    return (-1);
  }

  RELEASE(query);

  if ((row = sql_getrow(res, NULL)))
    result = atoi(row[0]);
  else
    result = 0;

  sql_free(res);

  return result;
}

static int 
__mydns_rr_load(SQL *sqlConn, MYDNS_RR **rptr, uint32_t zone,
		dns_qtype_t type,
		const char *name, const char *origin, const char *active, const char *filter) {
  char		*query = NULL;
  int		res;
  char		*columns = NULL;

  columns = mydns_rr_columns();

  query = mydns_rr_prepare_query(zone, type, name, origin, active, columns, filter);

  RELEASE(columns);

  res = __mydns_rr_do_load(sqlConn, rptr, query, origin);

  return res;
}

int mydns_rr_load_all(SQL *sqlConn, MYDNS_RR **rptr, uint32_t zone,
		      dns_qtype_t type,
		      const char *name, const char *origin) {

  return __mydns_rr_load(sqlConn, rptr, zone, type, name, origin, NULL, NULL);
}

int mydns_rr_load_active(SQL *sqlConn, MYDNS_RR **rptr, uint32_t zone,
			 dns_qtype_t type,
			 const char *name, const char *origin) {

  return __mydns_rr_load(sqlConn, rptr, zone, type, name, origin, mydns_rr_active_types[0], NULL);
}

int mydns_rr_load_inactive(SQL *sqlConn, MYDNS_RR **rptr, uint32_t zone,
			   dns_qtype_t type,
			   const char *name, const char *origin) {

  return __mydns_rr_load(sqlConn, rptr, zone, type, name, origin, mydns_rr_active_types[1], NULL);
}

int mydns_rr_load_deleted(SQL *sqlConn, MYDNS_RR **rptr, uint32_t zone,
			  dns_qtype_t type,
			  const char *name, const char *origin) {

  return __mydns_rr_load(sqlConn, rptr, zone, type, name, origin, mydns_rr_active_types[2], NULL);
}

int mydns_rr_count_all(SQL *sqlConn, uint32_t zone,
		       dns_qtype_t type,
		       const char *name, const char *origin) {

  return __mydns_rr_count(sqlConn, zone, type, name, origin, mydns_rr_active_types[0], NULL);
}

int mydns_rr_count_active(SQL *sqlConn, uint32_t zone,
			  dns_qtype_t type,
			  const char *name, const char *origin) {

  return __mydns_rr_count(sqlConn, zone, type, name, origin, mydns_rr_active_types[0], NULL);
}

int mydns_rr_count_inactive(SQL *sqlConn, uint32_t zone,
			    dns_qtype_t type,
			    const char *name, const char *origin) {

  return __mydns_rr_count(sqlConn, zone, type, name, origin, mydns_rr_active_types[1], NULL);
}

int mydns_rr_count_deleted(SQL *sqlConn, uint32_t zone,
			   dns_qtype_t type,
			   const char *name, const char *origin) {

  return __mydns_rr_count(sqlConn, zone, type, name, origin, mydns_rr_active_types[2], NULL);
}


int mydns_rr_load_all_filtered(SQL *sqlConn, MYDNS_RR **rptr, uint32_t zone,
			       dns_qtype_t type,
			       const char *name, const char *origin, const char *filter) {

  return __mydns_rr_load(sqlConn, rptr, zone, type, name, origin, NULL, filter);
}

int mydns_rr_load_active_filtered(SQL *sqlConn, MYDNS_RR **rptr, uint32_t zone,
				  dns_qtype_t type,
				  const char *name, const char *origin, const char *filter) {

  return __mydns_rr_load(sqlConn, rptr, zone, type, name, origin, mydns_rr_active_types[0], filter);
}

int mydns_rr_load_inactive_filtered(SQL *sqlConn, MYDNS_RR **rptr, uint32_t zone,
				    dns_qtype_t type,
				    const char *name, const char *origin, const char *filter) {

  return __mydns_rr_load(sqlConn, rptr, zone, type, name, origin, mydns_rr_active_types[1], filter);
}

int mydns_rr_load_deleted_filtered(SQL *sqlConn, MYDNS_RR **rptr, uint32_t zone,
				   dns_qtype_t type,
				   const char *name, const char *origin, const char *filter) {

  return __mydns_rr_load(sqlConn, rptr, zone, type, name, origin, mydns_rr_active_types[2], filter);
}

int mydns_rr_count_all_filtered(SQL *sqlConn, uint32_t zone,
				dns_qtype_t type,
				const char *name, const char *origin, const char *filter) {

  return __mydns_rr_count(sqlConn, zone, type, name, origin, mydns_rr_active_types[0], filter);
}

int mydns_rr_count_active_filtered(SQL *sqlConn, uint32_t zone,
				   dns_qtype_t type,
				   const char *name, const char *origin, const char *filter) {

  return __mydns_rr_count(sqlConn, zone, type, name, origin, mydns_rr_active_types[0], filter);
}

int mydns_rr_count_inactive_filtered(SQL *sqlConn, uint32_t zone,
				     dns_qtype_t type,
				     const char *name, const char *origin, const char *filter) {

  return __mydns_rr_count(sqlConn, zone, type, name, origin, mydns_rr_active_types[1], filter);
}

int mydns_rr_count_deleted_filtered(SQL *sqlConn, uint32_t zone,
				    dns_qtype_t type,
				    const char *name, const char *origin, const char *filter) {

  return __mydns_rr_count(sqlConn, zone, type, name, origin, mydns_rr_active_types[2], filter);
}

/*--- mydns_rr_load() ---------------------------------------------------------------------------*/

/* vi:set ts=3: */
