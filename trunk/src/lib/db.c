/**************************************************************************************************
	$Id: db.c,v 1.43 2006/01/18 20:46:47 bboy Exp $

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

/* Make this nonzero to enable debugging for this source file */
#define	DEBUG_DB	1



/**************************************************************************************************
	DB_CHECK_OPTIONAL
	Check optional columns.
**************************************************************************************************/
void
db_check_optional(void) {
  int		old_soa_use_active = mydns_soa_use_active;
  int		old_soa_use_xfer = mydns_soa_use_xfer;
  int		old_soa_use_update_acl = mydns_soa_use_update_acl;
  int		old_soa_use_recursive = mydns_soa_use_recursive;
  int		old_rr_use_active = mydns_rr_use_active;
  int		old_rr_use_stamp = mydns_rr_use_stamp;
  int		old_rr_use_serial = mydns_rr_use_serial;
  int		old_rr_extended_data = mydns_rr_extended_data;

  /* Check for soa.active */
  mydns_set_soa_use_active(sql);
  if (mydns_soa_use_active != old_soa_use_active)
    Verbose(_("optional 'active' column found in '%s' table"), mydns_soa_table_name);

  if (mydns_soa_use_active) mydns_soa_get_active_types(sql);

  /* Check for soa.xfer */
  mydns_set_soa_use_xfer(sql);
  if (mydns_soa_use_xfer != old_soa_use_xfer)
    Verbose(_("optional 'xfer' column found in '%s' table"), mydns_soa_table_name);

  /* Check for soa.update_acl */
  mydns_set_soa_use_update_acl(sql);
  if (mydns_soa_use_update_acl != old_soa_use_update_acl)
    Verbose(_("optional 'update_acl' column found in '%s' table"), mydns_soa_table_name);

  /* Check for soa.recursive */
  mydns_set_soa_use_recursive(sql);
  if (mydns_soa_use_recursive != old_soa_use_recursive)
    Verbose(_("optional 'recursive' column found in '%s' table"), mydns_soa_table_name);

  /* Check for rr.active */
  mydns_set_rr_use_active(sql);
  if (mydns_rr_use_active != old_rr_use_active)
    Verbose(_("optional 'active' column found in '%s' table"), mydns_rr_table_name);

  if (mydns_rr_use_active) mydns_rr_get_active_types(sql);

  /* Check for rr.stamp */
  mydns_set_rr_use_stamp(sql);
  if (mydns_rr_use_stamp != old_rr_use_stamp)
    Verbose(_("optional 'stamp' column found in '%s' table"), mydns_rr_table_name);

  /* Check for rr.serial */
  mydns_set_rr_use_serial(sql);
  if (mydns_rr_use_serial != old_rr_use_serial)
    Verbose(_("optional 'serial' column found in '%s' table"), mydns_rr_table_name);

  /* Check for rr.edata */
  mydns_set_rr_extended_data(sql);
  if (mydns_rr_extended_data != old_rr_extended_data)
    Verbose(_("optional 'edata' column found in '%s' table"), mydns_rr_table_name);
}
/*--- db_check_optional() -----------------------------------------------------------------------*/


/* vi:set ts=3: */
/* NEED_PO */
