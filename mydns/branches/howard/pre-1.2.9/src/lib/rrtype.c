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

#include "named.h"

#include "check.h"
#include "export.h"
#include "import.h"
#include "reply.h"
#include "rr.h"
#include "rrtype.h"
#include "update.h"

#if DEBUG_ENABLED
int		debug_rrtype = 0;
#endif

static dns_qtype_map **RR_TYPES_BY_ID = NULL;
static dns_qtype_map **RR_TYPES_BY_NAME = NULL;
static int RR_TYPES_ENTRIES = 0;

void mydns_rr_set_typemaps(dns_qtype_map **types_by_id, dns_qtype_map **types_by_name, int type_entries) {
#if DEBUG_ENABLED
  Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_set_typemaps called"));
#endif
  RR_TYPES_BY_ID = types_by_id;
  RR_TYPES_BY_NAME = types_by_name;
  RR_TYPES_ENTRIES = type_entries;
#if DEBUG_ENABLED
  Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_set_typemaps returns"));
#endif
}

int mydns_rr_get_typemap_entries() {
#if DEBUG_ENABLED
  Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_get_typemap_entries called"));
#endif
  return RR_TYPES_ENTRIES;
}

dns_qtype_map *mydns_rr_get_type_by_idx(int type_index) {
#if DEBUG_ENABLED
  Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_get_type_by_idx called"));
#endif
  if (type_index >= 0 && type_index < RR_TYPES_ENTRIES) {
#if DEBUG_ENABLED
  Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_get_type_by_idx returns type_entry"));
#endif
    return RR_TYPES_BY_NAME[type_index];
  } else {
#if DEBUG_ENABLED
  Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_get_type_by_idx returns NULL"));
#endif
    return NULL;
  }
}

/**************************************************************************************************
	MYDNS_RR_GET_TYPE_BY_ID
**************************************************************************************************/
dns_qtype_map
*mydns_rr_get_type_by_id(dns_qtype_t type) {
  register int i;

#if DEBUG_ENABLED
  Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_get_type_by_id called"));
#endif
  /* Replace by a binary chop or a biased binary chop or indexed lookup */
  for (i = 0; i < RR_TYPES_ENTRIES; i++) {
    if (type == RR_TYPES_BY_ID[i]->rr_type) {
#if DEBUG_ENABLED
      Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_get_type_by_id returns type_entry"));
#endif
      return RR_TYPES_BY_ID[i];
    }
  }

#if DEBUG_ENABLED
  Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_get_type_by_id returns NULL"));
#endif
  return NULL;
}
/*--- mydns_rr_get_type_by_name() ---------------------------------------------------------------*/

/**************************************************************************************************
	MYDNS_RR_GET_TYPE_BY_NAME
**************************************************************************************************/
dns_qtype_map
*mydns_rr_get_type_by_name(char *type) {
  register char *c;
  register int i;

#if DEBUG_ENABLED
  Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_get_type_by_name called"));
#endif
  for (c = type; *c; c++)
    *c = toupper(*c);

  /* Replace by a binary chop or a biased binary chop */
  for (i = 0; i < RR_TYPES_ENTRIES; i++) {
    if (strcmp(type, RR_TYPES_BY_NAME[i]->rr_type_name) == 0) {
#if DEBUG_ENABLED
      Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_get_type_by_name returns type_entry"));
#endif
      return RR_TYPES_BY_NAME[i];
    }
  }

#if DEBUG_ENABLED
  Debug(rrtype, DEBUGLEVEL_FUNCS, _("mydns_rr_get_type_by_name returns NULL"));
#endif
  return NULL;
}
/*--- mydns_rr_get_type_by_name() ---------------------------------------------------------------*/

