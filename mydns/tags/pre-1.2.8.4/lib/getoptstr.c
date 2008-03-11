/**************************************************************************************************
	$Id: getoptstr.c,v 1.11 2005/04/20 16:49:11 bboy Exp $

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

#include "mydnsutil.h"


/**************************************************************************************************
	GETOPTSTR
	Given a list of long opts, constructs the opt string automatically, since I always forget to
	update it.
**************************************************************************************************/
char *
getoptstr(struct option const longopts[]) {
  static char *optstr = NULL;
  char *c;
  register int n, opts;
  int optsize = 0;

  /* Count number of options */
  for (opts = 0; longopts[opts].name; opts++) {
    optsize += 1 + ((longopts[opts].has_arg == required_argument)
		    ?1
		    :(longopts[opts].has_arg == optional_argument)
		    ?2
		    :0);
  }

#if DEBUG_ENABLED
  Debug(_("Optstring is %d in size with %d elements"), optsize, opts);
#endif

  /* Allocate optstr */
  optstr = REALLOCATE(optstr, optsize + 1, char[]);
  memset(optstr, 0, optsize + 1);

  /* Build optstr */
  for (c = optstr, n = 0; n < opts && c - optstr < optsize; n++) {
    if (longopts[n].val) {
      *c++ = longopts[n].val;
      if (longopts[n].has_arg == required_argument)
	*c++ = ':';
      else if (longopts[n].has_arg == optional_argument) {
	*c++ = ':';
	*c++ = ':';
      }
    }
  }
  return (optstr);
}
/*--- getoptstr() -------------------------------------------------------------------------------*/

/* vi:set ts=3: */
