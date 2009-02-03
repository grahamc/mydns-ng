/**************************************************************************************************
	$Id: libptr.c,v 1.8 2005/04/20 17:22:25 bboy Exp $

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

#include "util.h"
#include "libptr.h"

/**************************************************************************************************
	This code used to be in libmydns.  However, the PTR table is now obsolete.
**************************************************************************************************/

char mydns_ptr_table_name[PATH_MAX] = MYDNS_PTR_TABLE;

/* Optional columns */
int mydns_ptr_use_active = 0;


/**************************************************************************************************
	MYDNS_PTR_COUNT
	Returns the number of zones in the ptr table.
**************************************************************************************************/
long
mydns_ptr_count(SQL *sqlConn)
{
	return sql_count(sqlConn, "SELECT COUNT(*) FROM %s", mydns_ptr_table_name);
}
/*--- mydns_ptr_count() -------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_SET_PTR_TABLE_NAME
**************************************************************************************************/
void
mydns_set_ptr_table_name(const char *name)
{
	if (!name)
		strncpy(mydns_ptr_table_name, MYDNS_PTR_TABLE, sizeof(mydns_ptr_table_name)-1);
	else
		strncpy(mydns_ptr_table_name, name, sizeof(mydns_ptr_table_name)-1);
}
/*--- mydns_set_ptr_table_name() ----------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_PARSE_PTR
**************************************************************************************************/
inline MYDNS_PTR *
mydns_parse_ptr(row)
	SQL_ROW row; {
  MYDNS_PTR *rv;
  register uint len;

  rv = (MYDNS_PTR *)ALLOCATE(sizeof(MYDNS_PTR), MYDNS_PTR);

  rv->next = NULL;

  rv->id = atou(row[0]);
  rv->ip = atou(row[1]);
  strncpy(rv->name, row[2], sizeof(rv->name)-1);
  rv->ttl = atou(row[3]);

  /* Add dot to end of name, if the user forgot */
  len = strlen(rv->name);
  if (rv->name[len-1] != '.' && len < sizeof(rv->name)-1) {
    rv->name[len] = '.';
    rv->name[len+1] = '\0';
  }

  return (rv);
}
/*-----------------------------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_PTR_DUP
**************************************************************************************************/
MYDNS_PTR *
mydns_ptr_dup(start, recurse)
     MYDNS_PTR *start;
     int recurse;
{
  register MYDNS_PTR *first = NULL, *last = NULL, *ptr, *s, *tmp;

  for (s = start; s; s = tmp) {
    tmp = s->next;

    ptr = (MYDNS_PTR *)ALLOCATE(sizeof(MYDNS_PTR), MYDNS_PRT);

    ptr->id = s->id;
    ptr->ip = s->ip;
    strncpy(ptr->name, s->name, sizeof(ptr->name)-1);
    ptr->ttl = s->ttl;
    ptr->next = NULL;
    if (recurse) {
      if (!first) first = ptr;
      if (last) last->next = ptr;
      last = ptr;
    } else
      return (ptr);
  }

  return (first);
}
/*-----------------------------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_PTR_SIZE
**************************************************************************************************/
inline size_t
mydns_ptr_size(first)
	MYDNS_PTR *first;
{
	register MYDNS_PTR *p;
	register size_t size = 0;

	for (p = first; p; p = p->next)
		size += sizeof(MYDNS_PTR) + strlen(p->name);

	return (size);
}
/*-----------------------------------------------------------------------------------------------*/


/**************************************************************************************************
	_MYDNS_PTR_FREE
	Frees the pointed-to structure.	Don't call this function directly, call the macro.
**************************************************************************************************/
inline void
_mydns_ptr_free(MYDNS_PTR *first)
{
	register MYDNS_PTR *p, *tmp;

	for (p = first; p; p = tmp)
	{
		tmp = p->next;
		RELEASE(p);
	}
}
/*-----------------------------------------------------------------------------------------------*/


/**************************************************************************************************
	MYDNS_PTR_LOAD
	Returns 0 on success or nonzero if an error occurred.
	SQL	*sql;						Open database connection
	MYDNS_PTR **rptr;				Result list will be returned here
	struct in_addr *addr;		Look up PTR record for this IP
**************************************************************************************************/
int
mydns_ptr_load(SQL *sqlConn, MYDNS_PTR **rptr, struct in_addr *addr)
{
	MYDNS_PTR *first = NULL, *last = NULL;
	size_t	 querylen;
	char		 query[DNS_QUERYBUFSIZ];
	SQL_RES	 *res;
	SQL_ROW	 row;

	if (rptr) *rptr = NULL;

	/* Verify args */
	if (!sqlConn || !rptr)
	{
		errno = EINVAL;
		return (-1);
	}

	/* Construct query */
	querylen = snprintf(query, sizeof(query),
		"SELECT "MYDNS_PTR_FIELDS"%s FROM %s WHERE ip=%u",
		(mydns_ptr_use_active ? ",active" : ""),
		mydns_ptr_table_name, addr->s_addr);

	/* Submit query */
	if (!(res = sql_query(sqlConn, query, querylen)))
		return (-1);

	/* Add results to list */
	while ((row = sql_getrow(res, NULL)))
	{
		MYDNS_PTR *new;

		if (mydns_ptr_use_active && row[4] && !GETBOOL(row[4]))
			continue;

		if (!(new = mydns_parse_ptr(row)))
		{
			sql_free(res);
			return (-1);
		}
		if (!first) first = new;
		if (last) last->next = new;
		last = new;
	}

	*rptr = first;
	sql_free(res);
	return (0);
}
/*-----------------------------------------------------------------------------------------------*/

/* vi:set ts=3: */
