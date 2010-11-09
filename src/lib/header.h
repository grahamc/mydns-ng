/**************************************************************************************************
	$Id: header.h,v 1.5 2005/04/20 16:43:22 bboy Exp $

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

#ifndef _MYDNS_HEADER_H
#define _MYDNS_HEADER_H

/* DNS_HEADER: The DNS message header structure */
typedef struct {
#if BYTE_ORDER != BIG_ENDIAN
  unsigned	rd :1;				/* Recursion Desired */
  unsigned	tc :1;				/* Truncation */
  unsigned	aa :1;				/* Authoritative Answer */
  unsigned	opcode :4;			/* Type of query */
  unsigned	qr :1;				/* Query/Response flag */
  unsigned	rcode :4;			/* Response code */
  unsigned	cd: 1;				/* Checking Disabled */
  unsigned	ad: 1;				/* Authentic Data */
  unsigned	z :1;				/* Unused */
  unsigned	ra :1;				/* Recursion Available */
#else
  unsigned	qr: 1;				/* Query/Response flag */
  unsigned	opcode: 4;			/* Type of query */
  unsigned	aa: 1;				/* Authoritative Answer */
  unsigned	tc: 1;				/* Truncation */
  unsigned	rd: 1;				/* Recursion Desired */
  unsigned	ra: 1;				/* Recursion Available */
  unsigned	z :1;				/* Unused */
  unsigned	ad: 1;				/* Authentic Data */
  unsigned	cd: 1;				/* Checking Disabled */
  unsigned	rcode :4;			/* Response code */
#endif
} DNS_HEADER;


#endif /* _MYDNS_HEADER_H */

/* vi:set ts=3: */
