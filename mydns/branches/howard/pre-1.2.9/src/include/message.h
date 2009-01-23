/**************************************************************************************************
	Copyright (C) 2009-  Howard Wilkinson <howard@cohtech.com>

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

#ifndef _MYDNS_LIB_MESSAGE_H
#define _MYDNS_LIB_MESSAGE_H

extern char		*dns_make_message(TASK *, uint16_t, uint8_t, dns_qtype_t,
					  char *, int , size_t *);

#define dns_make_question(t,id,qtype,name,rd,length) \
  dns_make_message((t),(id),DNS_OPCODE_QUERY,(qtype),(name),(rd),(length))
#define dns_make_notify(t,id,qtype,name,rd,length) \
  dns_make_message((t),(id),DNS_OPCODE_NOTIFY,(qtype),(name),(rd),(length))

#endif
