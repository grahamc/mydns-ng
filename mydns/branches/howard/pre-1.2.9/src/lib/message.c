/**************************************************************************************************
	$Id: question.c,v 1.6 2005/03/22 17:44:56 bboy Exp $

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

/* Set this to nonzero to enable debugging for this source file */
#define	MESSAGE_DEBUG	1


/**************************************************************************************************
	DNS_MAKE_MESSAGE
	Make a DNS message packet with the specified attributes.
	Returns a pointer to dynamic data containing the message or NULL on error.
	Sets 'length' to the length of the message packet.
**************************************************************************************************/
char *
dns_make_message(TASK * t, uint16_t id, uint8_t opcode, dns_qtype_t qtype,
		 char *name, int rd, size_t *length) {
  char		*message = NULL;				/* Message buffer */
  char		*dest = NULL;					/* Current destination in 'message' */
  DNS_HEADER	header;						/* DNS header */
  char		*mark = NULL;					/* Location of last label separator */
  register int	labels = 0;					/* Number of labels found */
  register char *c = NULL;
  int		messagesize = 0;

  memset(&header, 0, sizeof(header));

  if (t->protocol == SOCK_DGRAM) message = ALLOCATE(messagesize = DNS_MAXPACKETLEN_UDP, char[]);
  else if (t->protocol == SOCK_STREAM) message = ALLOCATE(messagesize = DNS_MAXPACKETLEN_TCP, char[]);
  else Err(_("unknown protocol %d"), t->protocol);

  dest = message;
  if (length)
    *length = 0;
  if (!name) {
    if (length) *length = (int)ERR_MALFORMED_REQUEST;
    RELEASE(message);
    return (NULL);
  }

  memset(&header, 0, sizeof(DNS_HEADER));
  DNS_PUT16(dest, id);						/* ID */
  header.opcode = opcode;					/* Opcode */
  header.rd = rd;						/* Recursion desired? */
  memcpy(dest, &header, sizeof(DNS_HEADER)); dest += SIZE16;
  DNS_PUT16(dest, 1);						/* QDCOUNT */
  DNS_PUT16(dest, 0);						/* ANCOUNT */
  DNS_PUT16(dest, 0);						/* NSCOUNT */
  DNS_PUT16(dest, 0);						/* ARCOUNT */

  for (mark = dest++, c = name; *c; c++) {			/* QNAME */
    if ((c - name) > DNS_MAXNAMELEN) {
      if (length) *length = (int)ERR_Q_NAME_TOO_LONG;
      RELEASE(message);
      return NULL;						/* Name too long */
    }
    if (*c != '.')						/* Append current character */
      *dest++ = *c;
    if (mark && (*c == '.' || !c[1])) {
      /* Store current label length at 'mark' */
      if ((*mark = dest - mark - 1) > DNS_MAXLABELLEN) {
	if (length) *length = (int)ERR_Q_LABEL_TOO_LONG;
	RELEASE(message);
	return NULL;	/* Label too long */
      }
      mark = dest++;
      labels++;
    }
    if (*c == '.' && !c[1]) {					/* Is this the end? */
      *mark = 0;
      break;
    }
    if ((dest - message) >= messagesize) {
      if (length) *length = (int)ERR_QUESTION_TRUNCATED;
      RELEASE(message);
      return NULL;
    }
  }
  DNS_PUT16(dest, (uint16_t)qtype);				/* QTYPE */
  DNS_PUT16(dest, DNS_CLASS_IN);				/* QCLASS */

  if (length) *length = dest - message;

  return (message);
}
/*--- dns_make_question() -----------------------------------------------------------------------*/

/* vi:set ts=3: */
