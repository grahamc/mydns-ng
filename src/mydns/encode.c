/**************************************************************************************************
	$Id: encode.c,v 1.36 2006/01/18 20:46:47 bboy Exp $

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
#define	DEBUG_ENCODE	1

/* Set this to nonzero to disable encoding */
#define	NO_ENCODING	0


/**************************************************************************************************
	NAME_REMEMBER
	Adds the specified name + offset to the `Labels' array within the specified task.
**************************************************************************************************/
int
name_remember(TASK *t, const char *name, unsigned int offset) {
  if (!name || strlen(name) > 64)			/* Don't store labels > 64 bytes in length */
    return (0);

#if DYNAMIC_NAMES
  t->Names = REALLOCATE(t->Names, (t->numNames + 1) * sizeof(char *), char*[]);
  t->Names[t->numNames] = STRDUP(name);
  t->Offsets = REALLOCATE(t->Offsets, (t->numNames + 1) * sizeof(unsigned int), unsigned int[]);
#else
  if (t->numNames >= MAX_STORED_NAMES - 1)
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_RR_NAME_TOO_LONG);
  strncpy(t->Names[t->numNames], name, sizeof(t->Names[t->numNames]) - 1);
#endif

  t->Offsets[t->numNames] = offset;
  t->numNames++;
  return (0);
}
/*--- name_remember() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	NAME_FORGET
	Forget all names in the specified task.
**************************************************************************************************/
inline void
name_forget(TASK *t) {
#if DYNAMIC_NAMES
  register int n = 0;

  for (n = 0; n < t->numNames; n++)
    RELEASE(t->Names[n]);
  RELEASE(t->Names);
  RELEASE(t->Offsets);
#endif
  t->numNames = 0;
}
/*--- name_forget() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	NAME_FIND
	Searches the task's remembered names arary for `name'.
	Returns the offset within the reply if found, or 0 if not found.
**************************************************************************************************/
unsigned int
name_find(TASK *t, const char *name) {
  register unsigned int n = 0;

  for (n = 0; n < t->numNames; n++)
    if (!strcasecmp(t->Names[n], name)) {
      return (t->Offsets[n]);
    }
  return (0);
}
/*--- name_find() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	NAME_ENCODE
	Encodes `in_name' into `dest'.  Returns the length of data in `dest', or -1 on error.
	If `name' is not NULL, it should be DNS_MAXNAMELEN bytes or bigger.
**************************************************************************************************/
int
name_encode(TASK *t, char *dest, const char *name, unsigned int dest_offset, int compression) {
  char			namebuf[DNS_MAXNAMELEN+1];
  register char		*c = NULL;
  char			*d = NULL;
  char			*this_name = NULL;
  char			*cp = NULL;
  register int		len = 0;
  register unsigned int	offset = 0;

  memset(&namebuf[0], 0, sizeof(namebuf));

  strncpy(namebuf, name, sizeof(namebuf)-1);

  /* Label must end in the root zone (with a dot) */
  if (LASTCHAR(namebuf) != '.')
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_NAME_FORMAT);

  /* Examine name one label at a time */
  for (c = namebuf, d = dest; *c; c++)
    if (c == namebuf || *c == '.') {
      if (!c[1]) {
	len++;
	if (len > DNS_MAXNAMELEN)
	  return dnserror(t, DNS_RCODE_SERVFAIL, ERR_RR_NAME_TOO_LONG);
	*d++ = 0;
	return (len);
      }
      this_name = (c == namebuf) ? c : (++c);

#if !NO_ENCODING
      if (compression && !t->no_markers && (offset = name_find(t, this_name))) {
	/* Found marker for this name - output offset pointer and we're done */
	len += SIZE16;
	if (len > DNS_MAXNAMELEN)
	  return dnserror(t, DNS_RCODE_SERVFAIL, ERR_RR_NAME_TOO_LONG);
	offset |= 0xC000;
	DNS_PUT16(d, offset);
	return (len);
      } else	/* No marker for this name; encode current label and store marker */
#endif
	{
	  register unsigned int nlen = 0;

	  if ((cp = strchr(this_name, '.')))
	    *cp = '\0';
	  nlen = strlen(this_name);
	  if (nlen > DNS_MAXLABELLEN)
	    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_RR_LABEL_TOO_LONG);
	  len += nlen + 1;
	  if (len > DNS_MAXNAMELEN)
	    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_RR_NAME_TOO_LONG);
	  *d++ = (unsigned char)nlen;
	  memcpy(d, this_name, nlen);
	  d += nlen;
	  if (cp)
	    *cp = '.';
	  if (!t->no_markers && (name_remember(t, this_name, dest_offset + (c - namebuf)) < 0))
	    return (-1);
	}
    }
  return (len);
}

int
name_encode2(TASK *t, char **dest, const char *name, unsigned int dest_offset, int compression) {
  char			*namebuf = NULL;
  register char		*c = NULL, *d = NULL, *this_name = NULL, *cp = NULL;
  register int		len = 0;
  register unsigned int	offset = 0;

  namebuf = STRDUP(name);

  /* Label must end in the root zone (with a dot) */
  if (LASTCHAR(namebuf) != '.')
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_NAME_FORMAT);

  *dest = ALLOCATE(strlen(namebuf) + 1, char[]); /* If not compressing this should be equal
						    If compressing then it should be bigger */

  /* Examine name one label at a time */
  for (c = namebuf, d = *dest; *c; c++) {
    if (c == namebuf || *c == '.') {
      if (!c[1]) {
	RELEASE(namebuf);
	len++;
	if (len > DNS_MAXNAMELEN) {
	  RELEASE(*dest);
	  return dnserror(t, DNS_RCODE_SERVFAIL, ERR_RR_NAME_TOO_LONG);
	}
	*d++ = 0;
	return (len);
      }
      this_name = (c == namebuf) ? c : (++c);

#if !NO_ENCODING
      if (compression && !t->no_markers && (offset = name_find(t, this_name))) {
	RELEASE(namebuf);
	/* Found marker for this name - output offset pointer and we're done */
	len += SIZE16;
	if (len > DNS_MAXNAMELEN) {
	  RELEASE(*dest);
	  return dnserror(t, DNS_RCODE_SERVFAIL, ERR_RR_NAME_TOO_LONG);
	}
	offset |= 0xC000;
	DNS_PUT16(d, offset);
	return (len);
      } else {	/* No marker for this name; encode current label and store marker */
#endif
	  register unsigned int nlen;

	  if ((cp = strchr(this_name, '.')))
	    *cp = '\0';
	  nlen = strlen(this_name);
	  if (nlen > DNS_MAXLABELLEN) {
	    RELEASE(*dest);
	    RELEASE(namebuf);
	    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_RR_LABEL_TOO_LONG);
	  }
	  len += nlen + 1;
	  if (len > DNS_MAXNAMELEN) {
	    RELEASE(*dest);
	    RELEASE(namebuf);
	    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_RR_NAME_TOO_LONG);
	  }
	  *d++ = (unsigned char)nlen;
	  memcpy(d, this_name, nlen);
	  d += nlen;
	  if (cp)
	    *cp = '.';
	  if (!t->no_markers && (name_remember(t, this_name, dest_offset + (c - namebuf)) < 0)) {
	    RELEASE(*dest);
	    RELEASE(namebuf);
	    return (-1);
	  }
	}
    }
  }
  RELEASE(namebuf);
  return (len);
}
/*--- name_encode() -----------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
