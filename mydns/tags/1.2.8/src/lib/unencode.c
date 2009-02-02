/**************************************************************************************************
	$Id: unencode.c,v 1.8 2006/01/18 20:46:46 bboy Exp $

	unencode.c: Routine to unencode a DNS-encoded name.

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


/**************************************************************************************************
	NAME_UNENCODE
	Get a name from a compressed question.
	Returns the new location of src, or NULL if an error occurred.
	If an error occurs, puts a descriptive message in `dest'.
**************************************************************************************************/
uchar *
name_unencode(uchar *start, size_t slen, uchar *current, uchar *dest, size_t destsize) {
  register uchar *s = current;				/* Current pointer into input */
  register uchar *d = dest;				/* Current pointer into destination */
  register uchar n;
  register uchar len;
  register uchar *rp = NULL;

#define CHECK_DEST_SPACE(n)  \
  if (d >= dest+(destsize-(n))) {		 \
    dest[0] = ERR_Q_BUFFER_OVERFLOW;		 \
    return (NULL);				 \
  }						 \
  
  if (*s == 0) {						/* The name is just "." */
    CHECK_DEST_SPACE(2);
    *d++ = '.';
    *d = '\0';
    return (s+1);
  }

  while ((len = *s)) {						/* Get length octet */
    register uchar mask = len & 0xC0;

    if (mask == 0xC0) {
      uint16_t current_offset, new_offset;

      current_offset = s - start;

      DNS_GET16(new_offset, s);
      new_offset &= ~0xC000;

      if (((start + new_offset) < start)			/* Pointing out-of-bounds */
	  || (current_offset == new_offset)			/* Pointing to current position */
	  || ((start + new_offset) > start + slen)) {		/* Pointing out-of-bounds */
	dest[0] = ERR_Q_INVALID_COMPRESSION;
	return (NULL);
      }

      if (!rp)
	rp = s;

      s = start + new_offset;
    } else if (mask == 0x40 || mask == 0x80) {
      dest[0] = ERR_Q_INVALID_COMPRESSION;
      return (NULL);
    } else {
      s++;
      if (len + (s - current) > DNS_MAXNAMELEN) {
	dest[0] = ERR_Q_NAME_TOO_LONG;
	return (NULL);
      }
      for (n = 0; n < len; n++) {				/* Get label */
	CHECK_DEST_SPACE(1);
	*d++ = tolower(*s++);
      }
      CHECK_DEST_SPACE(1);
      *d++ = '.';
    }
  }
  CHECK_DEST_SPACE(1);
  *d = '\0';
  return (rp ? rp : s+1);
}
/*--- name_unencode() ---------------------------------------------------------------------------*/

/**************************************************************************************************
	NAME_UNENCODE2
	Get a name from a compressed question.
	Returns the new location of src, or NULL if an error occurred.
	If an error occurs, puts a descriptive message in `dest'.
**************************************************************************************************/
uchar *
name_unencode2(uchar *start, size_t slen, uchar **current, task_error_t *errcode) {
  uchar		*dest = NULL;
  int		destlen = 16;
  register uchar *s = *current;				/* Current pointer into input */
  register uchar *d;					/* Current pointer into destination */
  register uchar n;
  register uchar len;
  register uchar *rp = NULL;

#define CHECK_DEST_SPACE2(n)  \
  if (d >= dest+(destlen-(n))) {		 	\
    int doffset = d - dest;			 	\
    destlen += (n > 16)?n:16;			 	\
    if (!(dest = REALLOCATE(dest, destlen, char[]))) {	\
      *errcode = ERR_Q_BUFFER_OVERFLOW;		 	\
      return (NULL);				 	\
    }						 	\
    d = &dest[doffset];				 	\
  }						 	\
  
  dest = ALLOCATE(destlen, char[]);
  d = dest;

  if (*s == 0) {						/* The name is just "." */
    CHECK_DEST_SPACE2(2);
    *d++ = '.';
    *d = '\0';
    *current = s+1;
    return dest;
  }

  while ((len = *s)) {						/* Get length octet */
    register uchar mask = len & 0xC0;

    if (mask == 0xC0) {
      uint16_t current_offset, new_offset;

      current_offset = s - start;

      DNS_GET16(new_offset, s);
      new_offset &= ~0xC000;

      if (((start + new_offset) < start)			/* Pointing out-of-bounds */
	  || (current_offset == new_offset)			/* Pointing to current position */
	  || ((start + new_offset) > start + slen)) {		/* Pointing out-of-bounds */
	*errcode = ERR_Q_INVALID_COMPRESSION;
	RELEASE(dest);
	return (NULL);
      }

      if (!rp)
	rp = s;

      s = start + new_offset;
    } else if (mask == 0x40 || mask == 0x80) {
      *errcode = ERR_Q_INVALID_COMPRESSION;
      RELEASE(dest);
      return (NULL);
    } else {
      s++;
      if (len + (s - *current) > DNS_MAXNAMELEN) {
	*errcode = ERR_Q_NAME_TOO_LONG;
	RELEASE(dest);
	return (NULL);
      }
      for (n = 0; n < len; n++) {				/* Get label */
	CHECK_DEST_SPACE2(1);
	*d++ = tolower(*s++);
      }
      CHECK_DEST_SPACE2(1);
      *d++ = '.';
    }
  }
  CHECK_DEST_SPACE2(1);
  *d = '\0';
  *current = (rp ? rp : s+1);
  return dest; /* Could realloc to string length here but should be a small overhead so ignore */
}
/*--- name_unencode() ---------------------------------------------------------------------------*/

/* vi:set ts=3: */
