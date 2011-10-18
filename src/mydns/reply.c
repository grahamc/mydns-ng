/**************************************************************************************************
	$Id: reply.c,v 1.65 2006/01/18 20:46:47 bboy Exp $

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
#define	DEBUG_REPLY	1

#if DEBUG_ENABLED && DEBUG_REPLY
/* Strings describing the datasections */
const char *reply_datasection_str[] = { "QUESTION", "ANSWER", "AUTHORITY", "ADDITIONAL" };
#endif


/**************************************************************************************************
	REPLY_INIT
	Examines the question data, storing the name offsets (from DNS_HEADERSIZE) for compression.
**************************************************************************************************/
int
reply_init(TASK *t) {
  register char *c = NULL;						/* Current character in name */

  /* Examine question data, save labels found therein. The question data should begin with
     the name we've already parsed into t->qname.  I believe it is safe to assume that no
     compression will be possible in the question. */
  for (c = t->qname; *c; c++)
    if ((c == t->qname || *c == '.') && c[1])
      if (name_remember(t, (c == t->qname) ? c : (c+1),
			(((c == t->qname) ? c : (c+1)) - t->qname) + DNS_HEADERSIZE) < -1)
	return (-1);
  return (0);
}
/*--- reply_init() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_ADD_ADDITIONAL
	Add ADDITIONAL for each item in the provided list.
**************************************************************************************************/
static void
reply_add_additional(TASK *t, RRLIST *rrlist) {
  register RR *p = NULL;

  if (!rrlist)
    return;

  /* Examine each RR in the rrlist */
  for (p = rrlist->head; p; p = p->next) {
    if (p->rrtype == DNS_RRTYPE_RR) {
      MYDNS_RR *rr = (MYDNS_RR *)p->rr;
      if (rr->type == DNS_QTYPE_NS || rr->type == DNS_QTYPE_MX || rr->type == DNS_QTYPE_SRV) {
	(void)resolve(t, ADDITIONAL, DNS_QTYPE_A, MYDNS_RR_DATA_VALUE(rr), 0);
      }	else if (rr->type == DNS_QTYPE_CNAME) {
	/* Don't do this */
	(void)resolve(t, ADDITIONAL, DNS_QTYPE_CNAME, MYDNS_RR_DATA_VALUE(rr), 0);
      }
    }
    t->sort_level++;
  }
}
/*--- reply_add_additional() --------------------------------------------------------------------*/


/**************************************************************************************************
	RDATA_ENLARGE
	Expands t->rdata by `size' bytes.  Returns a pointer to the destination.
**************************************************************************************************/
static char *
rdata_enlarge(TASK *t, size_t size) {
  if (!size)
    return (NULL);

  t->rdlen += size;
  t->rdata = REALLOCATE(t->rdata, t->rdlen, char[]);
  return (t->rdata + t->rdlen - size);
}
/*--- rdata_enlarge() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_START_RR
	Begins an RR.  Appends to t->rdata all the header fields prior to rdlength.
	Returns the numeric offset of the start of this record within the reply, or -1 on error.
**************************************************************************************************/
static int
reply_start_rr(TASK *t, RR *r, const char *name, dns_qtype_t type, uint32_t ttl, const char *desc) {
  char	*enc = NULL;
  char	*dest = NULL;
  int	enclen = 0;

  /* name_encode returns dnserror() */
  if ((enclen = name_encode2(t, &enc, name, t->replylen + t->rdlen, 1)) < 0) {
    return rr_error(r->id, _("rr %u: %s (%s %s) (name=\"%s\")"), r->id,
		    _("invalid name in \"name\""), desc, _("record"), name);
  }

  r->length = enclen + SIZE16 + SIZE16 + SIZE32;

  if (!(dest = rdata_enlarge(t, r->length))) {
    RELEASE(enc);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_INTERNAL);
  }

  r->offset = dest - t->rdata + DNS_HEADERSIZE + t->qdlen;

  DNS_PUT(dest, enc, enclen);
  RELEASE(enc);
  DNS_PUT16(dest, type);
#if STATUS_ENABLED
  if (r->rrtype == DNS_RRTYPE_RR && r->rr)
    DNS_PUT16(dest, ((MYDNS_RR *)(r->rr))->class)
    else
#endif
      DNS_PUT16(dest, DNS_CLASS_IN);
  DNS_PUT32(dest, ttl);
  return (0);
}
/*--- reply_start_rr() --------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_ADD_GENERIC_RR
	Adds a generic resource record whose sole piece of data is a domain-name,
	or a 16-bit value plus a domain-name.
	Returns the numeric offset of the start of this record within the reply, or -1 on error.
**************************************************************************************************/
static inline int
reply_add_generic_rr(TASK *t, RR *r, const char *desc) {
  char		*enc = NULL, *dest = NULL;
  int		size = 0, enclen = 0;
  MYDNS_RR	*rr = (MYDNS_RR *)r->rr;

  if (reply_start_rr(t, r, (char*)r->name, rr->type, rr->ttl, desc) < 0)
    return (-1);

  if ((enclen = name_encode2(t, &enc, MYDNS_RR_DATA_VALUE(rr), CUROFFSET(t), 1)) < 0) {
    return rr_error(r->id, _("rr %u: %s (%s) (data=\"%s\")"), r->id,
		    _("invalid name in \"data\""), desc, (char*)MYDNS_RR_DATA_VALUE(rr));
  }

  size = enclen;
  r->length += SIZE16 + size;

  if (!(dest = rdata_enlarge(t, SIZE16 + size))) {
    RELEASE(enc);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_INTERNAL);
  }

  DNS_PUT16(dest, size);
  DNS_PUT(dest, enc, enclen);
  RELEASE(enc);
  return (0);
}
/*--- reply_add_generic_rr() --------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_ADD_A
	Adds an A record to the reply.
	Returns the numeric offset of the start of this record within the reply, or -1 on error.
**************************************************************************************************/
static inline int
reply_add_a(TASK *t, RR *r) {
  char		*dest = NULL;
  int		size = 0;
  MYDNS_RR	*rr = (MYDNS_RR *)r->rr;
  struct in_addr addr;
  uint32_t	ip = 0;

  memset(&addr, 0, sizeof(addr));

  if (inet_pton(AF_INET, MYDNS_RR_DATA_VALUE(rr), (void *)&addr) <= 0) {
    dnserror(t, DNS_RCODE_SERVFAIL, ERR_INVALID_ADDRESS);
    return rr_error(r->id, _("rr %u: %s (A %s) (address=\"%s\")"), r->id,
		    _("invalid address in \"data\""), _("record"), (char*)MYDNS_RR_DATA_VALUE(rr));
  }
  ip = ntohl(addr.s_addr);

  if (reply_start_rr(t, r, (char*)r->name, DNS_QTYPE_A, rr->ttl, "A") < 0)
    return (-1);

  size = SIZE32;
  r->length += SIZE16 + size;

  if (!(dest = rdata_enlarge(t, SIZE16 + size)))
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_INTERNAL);

  DNS_PUT16(dest, size);
  DNS_PUT32(dest, ip);

  return (0);
}
/*--- reply_add_a() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_ADD_AAAA
	Adds an AAAA record to the reply.
	Returns the numeric offset of the start of this record within the reply, or -1 on error.
**************************************************************************************************/
static inline int
reply_add_aaaa(TASK *t, RR *r) {
  char		*dest = NULL;
  int		size = 0;
  MYDNS_RR	*rr = (MYDNS_RR *)r->rr;
  uint8_t	addr[16];

  memset(&addr, 0, sizeof(addr));

  if (inet_pton(AF_INET6, MYDNS_RR_DATA_VALUE(rr), (void *)&addr) <= 0) {
    dnserror(t, DNS_RCODE_SERVFAIL, ERR_INVALID_ADDRESS);
    return rr_error(r->id, _("rr %u: %s (AAAA %s) (address=\"%s\")"), r->id,
		    _("invalid address in \"data\""), _("record"), (char*)MYDNS_RR_DATA_VALUE(rr));
  }

  if (reply_start_rr(t, r, (char*)r->name, DNS_QTYPE_AAAA, rr->ttl, "AAAA") < 0)
    return (-1);

  size = sizeof(uint8_t) * 16;
  r->length += SIZE16 + size;

  if (!(dest = rdata_enlarge(t, SIZE16 + size)))
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_INTERNAL);

  DNS_PUT16(dest, size);
  memcpy(dest, &addr, size);
  dest += size;

  return (0);
}
/*--- reply_add_aaaa() --------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_ADD_HINFO
	Adds an HINFO record to the reply.
	Returns the numeric offset of the start of this record within the reply, or -1 on error.
**************************************************************************************************/
static int
reply_add_hinfo(TASK *t, RR *r) {
  char		*dest = NULL;
  size_t	oslen = 0, cpulen = 0;
  MYDNS_RR	*rr = (MYDNS_RR *)r->rr;
  char		os[DNS_MAXNAMELEN + 1] = "", cpu[DNS_MAXNAMELEN + 1] = "";

  if (hinfo_parse(MYDNS_RR_DATA_VALUE(rr), cpu, os, DNS_MAXNAMELEN) < 0) {
    dnserror(t, DNS_RCODE_SERVFAIL, ERR_RR_NAME_TOO_LONG);
    return rr_error(r->id, _("rr %u: %s (HINFO %s) (data=\"%s\")"), r->id,
		    _("name too long in \"data\""), _("record"), (char*)MYDNS_RR_DATA_VALUE(rr));
  }

  cpulen = strlen(cpu);
  oslen = strlen(os);

  if (reply_start_rr(t, r, (char*)r->name, DNS_QTYPE_HINFO, rr->ttl, "HINFO") < 0)
    return (-1);

  r->length += SIZE16 + cpulen + oslen + 2;

  if (!(dest = rdata_enlarge(t, SIZE16 + cpulen + SIZE16 + oslen)))
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_INTERNAL);

  DNS_PUT16(dest, cpulen + oslen + 2);

  *dest++ = cpulen;
  memcpy(dest, cpu, cpulen);
  dest += cpulen;

  *dest++ = oslen;
  memcpy(dest, os, oslen);
  dest += oslen;

  return (0);
}
/*--- reply_add_hinfo() -------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_ADD_MX
	Adds an MX record to the reply.
	Returns the numeric offset of the start of this record within the reply, or -1 on error.
**************************************************************************************************/
static inline int
reply_add_mx(TASK *t, RR *r) {
  char		*enc = NULL, *dest = NULL;
  int		size = 0, enclen = 0;
  MYDNS_RR	*rr = (MYDNS_RR *)r->rr;

  if (reply_start_rr(t, r, (char*)r->name, DNS_QTYPE_MX, rr->ttl, "MX") < 0)
    return (-1);

  if ((enclen = name_encode2(t, &enc, MYDNS_RR_DATA_VALUE(rr), CUROFFSET(t) + SIZE16, 1)) < 0) {
    return rr_error(r->id, _("rr %u: %s (MX %s) (data=\"%s\")"), r->id,
		    _("invalid name in \"data\""), _("record"), (char*)MYDNS_RR_DATA_VALUE(rr));
  }

  size = SIZE16 + enclen;
  r->length += SIZE16 + size;

  if (!(dest = rdata_enlarge(t, SIZE16 + size))) {
    RELEASE(enc);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_INTERNAL);
  }

  DNS_PUT16(dest, size);
  DNS_PUT16(dest, (uint16_t)rr->aux);
  DNS_PUT(dest, enc, enclen);
  RELEASE(enc);
  return (0);
}
/*--- reply_add_mx() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_ADD_NAPTR
	Adds an NAPTR record to the reply.
	Returns the numeric offset of the start of this record within the reply, or -1 on error.
**************************************************************************************************/
static inline int
reply_add_naptr(TASK *t, RR *r) {
  MYDNS_RR	*rr = (MYDNS_RR *)r->rr;
  size_t	flags_len = 0, service_len = 0, regex_len = 0;
  char		*enc = NULL, *dest = NULL;
  int		size = 0, enclen = 0, offset = 0;

  flags_len = sizeof(MYDNS_RR_NAPTR_FLAGS(rr));
  service_len = strlen(MYDNS_RR_NAPTR_SERVICE(rr));
  regex_len = strlen(MYDNS_RR_NAPTR_REGEX(rr));

  if (reply_start_rr(t, r, (char*)r->name, DNS_QTYPE_NAPTR, rr->ttl, "NAPTR") < 0)
    return (-1);

  /* We are going to write "something else" and then a name, just like an MX record or something.
     In this case, though, the "something else" is lots of data.  Calculate the size of
     "something else" in 'offset' */
  offset = SIZE16 + SIZE16 + 1 + flags_len + 1 + service_len + 1 + regex_len;

  /* Encode the name at the offset */
  if ((enclen = name_encode2(t, &enc, MYDNS_RR_NAPTR_REPLACEMENT(rr), CUROFFSET(t) + offset, 1)) < 0) {
    return rr_error(r->id, _("rr %u: %s (NAPTR %s) (%s=\"%s\")"), r->id,
		    _("invalid name in \"replacement\""), _("record"), _("replacement"),
		    MYDNS_RR_NAPTR_REPLACEMENT(rr));
  }

  size = offset + enclen;
  r->length += SIZE16 + size;

  if (!(dest = rdata_enlarge(t, SIZE16 + size))) {
    RELEASE(enc);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_INTERNAL);
  }

  DNS_PUT16(dest, size);
  DNS_PUT16(dest, (uint16_t)MYDNS_RR_NAPTR_ORDER(rr));
  DNS_PUT16(dest, (uint16_t)MYDNS_RR_NAPTR_PREF(rr));

  *dest++ = flags_len;
  memcpy(dest, MYDNS_RR_NAPTR_FLAGS(rr), flags_len);
  dest += flags_len;

  *dest++ = service_len;
  memcpy(dest, MYDNS_RR_NAPTR_SERVICE(rr), service_len);
  dest += service_len;

  *dest++ = regex_len;
  memcpy(dest, MYDNS_RR_NAPTR_REGEX(rr), regex_len);
  dest += regex_len;

  DNS_PUT(dest, enc, enclen);
  RELEASE(enc);

  return (0);
}
/*--- reply_add_naptr() -------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_ADD_RP
	Adds an RP record to the reply.
	Returns the numeric offset of the start of this record within the reply, or -1 on error.
**************************************************************************************************/
static inline int
reply_add_rp(TASK *t, RR *r) {
  char		*mbox = NULL, *txt = NULL, *dest = NULL;
  char		*encmbox = NULL, *enctxt = NULL;
  int		size = 0, mboxlen = 0, txtlen = 0;
  MYDNS_RR	*rr = (MYDNS_RR *)r->rr;

  mbox = MYDNS_RR_DATA_VALUE(rr);
  txt = MYDNS_RR_RP_TXT(rr);

  if (reply_start_rr(t, r, (char*)r->name, DNS_QTYPE_RP, rr->ttl, "RP") < 0)
    return (-1);

  if ((mboxlen = name_encode2(t, &encmbox, mbox, CUROFFSET(t), 1)) < 0) {
    return rr_error(r->id, _("rr %u: %s (RP %s) (mbox=\"%s\")"), r->id,
		    _("invalid name in \"mbox\""), _("record"), mbox);
  }

  if ((txtlen = name_encode2(t, &enctxt, txt, CUROFFSET(t) + mboxlen, 1)) < 0) {
    RELEASE(encmbox);
    return rr_error(r->id, _("rr %u: %s (RP %s) (txt=\"%s\")"), r->id,
		    _("invalid name in \"txt\""), _("record"), txt);
  }

  size = mboxlen + txtlen;
  r->length += SIZE16 + size;

  if (!(dest = rdata_enlarge(t, SIZE16 + size))) {
    RELEASE(encmbox);
    RELEASE(enctxt);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_INTERNAL);
  }

  DNS_PUT16(dest, size);
  DNS_PUT(dest, encmbox, mboxlen);
  DNS_PUT(dest, enctxt, txtlen);
  RELEASE(encmbox);
  RELEASE(enctxt);
  return (0);
}
/*--- reply_add_rp() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_ADD_SOA
	Add a SOA record to the reply.
	Returns the numeric offset of the start of this record within the reply, or -1 on error.
**************************************************************************************************/
static inline int
reply_add_soa(TASK *t, RR *r) {
  char		*dest = NULL, *ns = NULL, *mbox = NULL;
  int		size = 0, nslen = 0, mboxlen = 0;
  MYDNS_SOA	*soa = (MYDNS_SOA *)r->rr;

  if (reply_start_rr(t, r, (char*)r->name, DNS_QTYPE_SOA, soa->ttl, "SOA") < 0)
    return (-1);

  if ((nslen = name_encode2(t, &ns, soa->ns, CUROFFSET(t), 1)) < 0) {
    return rr_error(r->id, _("rr %u: %s (SOA %s) (ns=\"%s\")"), r->id,
		    _("invalid name in \"ns\""), _("record"), soa->ns);
  }

  if ((mboxlen = name_encode2(t, &mbox, soa->mbox, CUROFFSET(t) + nslen, 1)) < 0) {
    RELEASE(ns);
    return rr_error(r->id, _("rr %u: %s (SOA %s) (mbox=\"%s\")"), r->id,
		    _("invalid name in \"mbox\""), _("record"), soa->mbox);
  }

  size = nslen + mboxlen + (SIZE32 * 5);
  r->length += SIZE16 + size;

  if (!(dest = rdata_enlarge(t, SIZE16 + size))) {
    RELEASE(ns);
    RELEASE(mbox);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_INTERNAL);
  }

  DNS_PUT16(dest, size);
  DNS_PUT(dest, ns, nslen);
  DNS_PUT(dest, mbox, mboxlen);
  RELEASE(ns);
  RELEASE(mbox);
  DNS_PUT32(dest, soa->serial);
  DNS_PUT32(dest, soa->refresh);
  DNS_PUT32(dest, soa->retry);
  DNS_PUT32(dest, soa->expire);
  DNS_PUT32(dest, soa->minimum);
  return (0);
}
/*--- reply_add_soa() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_ADD_SRV
	Adds a SRV record to the reply.
	Returns the numeric offset of the start of this record within the reply, or -1 on error.
**************************************************************************************************/
static inline int
reply_add_srv(TASK *t, RR *r) {
  char		*enc = NULL, *dest = NULL;
  int		size = 0, enclen = 0;
  MYDNS_RR	*rr = (MYDNS_RR *)r->rr;

  if (reply_start_rr(t, r, (char*)r->name, DNS_QTYPE_SRV, rr->ttl, "SRV") < 0)
    return (-1);

  /* RFC 2782 says that we can't use name compression on this field... */
  /* Arnt Gulbrandsen advises against using compression in the SRV target, although
     most clients should support it */
  if ((enclen = name_encode2(t, &enc, MYDNS_RR_DATA_VALUE(rr), CUROFFSET(t) + SIZE16 + SIZE16 + SIZE16, 0)) < 0) {
    return rr_error(r->id, _("rr %u: %s (SRV %s) (data=\"%s\")"), r->id,
		    _("invalid name in \"data\""), _("record"), (char*)MYDNS_RR_DATA_VALUE(rr));
  }

  size = SIZE16 + SIZE16 + SIZE16 + enclen;
  r->length += SIZE16 + size;

  if (!(dest = rdata_enlarge(t, SIZE16 + size))) {
    RELEASE(enc);
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_INTERNAL);
  }

  DNS_PUT16(dest, size);
  DNS_PUT16(dest, (uint16_t)rr->aux);
  DNS_PUT16(dest, (uint16_t)MYDNS_RR_SRV_WEIGHT(rr));
  DNS_PUT16(dest, (uint16_t)MYDNS_RR_SRV_PORT(rr));
  DNS_PUT(dest, enc, enclen);
  RELEASE(enc);
  return (0);
}
/*--- reply_add_srv() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_ADD_TXT
	Adds a TXT record to the reply.
	Returns the numeric offset of the start of this record within the reply, or -1 on error.
**************************************************************************************************/
static inline int
reply_add_txt(TASK *t, RR *r) {
  char		*dest = NULL;
  size_t	size = 0;
  size_t	len = 0;
  MYDNS_RR	*rr = (MYDNS_RR *)r->rr;

  len = MYDNS_RR_DATA_LENGTH(rr);

  if (reply_start_rr(t, r, (char*)r->name, DNS_QTYPE_TXT, rr->ttl, "TXT") < 0)
    return (-1);

  size = len + 1;
  r->length += SIZE16 + size;

  if (!(dest = rdata_enlarge(t, SIZE16 + size)))
    return dnserror(t, DNS_RCODE_SERVFAIL, ERR_INTERNAL);

  DNS_PUT16(dest, size);
  *dest++ = len;
  memcpy(dest, MYDNS_RR_DATA_VALUE(rr), len);
  dest += len;
  return (0);
}
/*--- reply_add_txt() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_PROCESS_RRLIST
	Adds each resource record found in `rrlist' to the reply.
**************************************************************************************************/
static int
reply_process_rrlist(TASK *t, RRLIST *rrlist) {
  register RR *r = NULL;

  if (!rrlist)
    return (0);

  for (r = rrlist->head; r; r = r->next) {
    switch (r->rrtype) {
    case DNS_RRTYPE_SOA:
      if (reply_add_soa(t, r) < 0)
	return (-1);
      break;

    case DNS_RRTYPE_RR:
      {
	MYDNS_RR *rr = (MYDNS_RR *)r->rr;

	if (!rr)
	  break;

	switch (rr->type) {
	case DNS_QTYPE_UNKNOWN:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unexpected resource record type - logic problem"));
	  break;

	case DNS_QTYPE_NONE:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unexpected resource record type - logic problem"));
	  break;

	case DNS_QTYPE_A:
	  if (reply_add_a(t, r) < 0)
	    return (-1);
	  break;

	case DNS_QTYPE_NS:
	  if (reply_add_generic_rr(t, r, "NS") < 0)
	    return (-1);
	  break;

	case DNS_QTYPE_MD:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_MF:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_CNAME:
	  if (reply_add_generic_rr(t, r, "CNAME") < 0)
	    return (-1);
	  break;

	case DNS_QTYPE_SOA:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unexpected resource record type - logic problem"));
	  break;

	case DNS_QTYPE_MB:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_MG:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_MR:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_NULL:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_WKS:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_PTR:
	  if (reply_add_generic_rr(t, r, "PTR") < 0)
	    return (-1);
	  break;

	case DNS_QTYPE_HINFO:
	  if (reply_add_hinfo(t, r) < 0)
	    return (-1);
	  break;

	case DNS_QTYPE_MINFO:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_MX:
	  if (reply_add_mx(t, r) < 0)
	    return (-1);
	  break;

	case DNS_QTYPE_TXT:
	  if (reply_add_txt(t, r) < 0)
	    return (-1);
	  break;

	case DNS_QTYPE_RP:
	  if (reply_add_rp(t, r) < 0)
	    return (-1);
	  break;

	case DNS_QTYPE_AFSDB:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_X25:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_ISDN:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_RT:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_NSAP:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_NSAP_PTR:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_SIG:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_KEY:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_PX:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_GPOS:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_AAAA:
	  if (reply_add_aaaa(t, r) < 0)
	    return (-1);
	  break;

	case DNS_QTYPE_LOC:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_NXT:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_EID:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_NIMLOC:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_SRV:
	  if (reply_add_srv(t, r) < 0)
	    return (-1);
	  break;

	case DNS_QTYPE_ATMA:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_NAPTR:
	  if (reply_add_naptr(t, r) < 0)
	    return (-1);
	  break;

	case DNS_QTYPE_KX:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_CERT:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_A6:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_DNAME:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_SINK:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_OPT:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_APL:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_DS:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_SSHFP:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_IPSECKEY:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_RRSIG:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_NSEC:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_DNSKEY:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_DHCID:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_NSEC3:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_NSEC3PARAM:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_HIP:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_SPF:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_UINFO:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_UID:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_GID:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_UNSPEC:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_TKEY:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_TSIG:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_IXFR:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unexpect resource record type - logic problem"));
	  break;

	case DNS_QTYPE_AXFR:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unexpected resource record type - logic problem"));
	  break;

	case DNS_QTYPE_MAILB:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_MAILA:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_ANY:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unexpected resource record type - logic problem"));
	  break;

	case DNS_QTYPE_TA:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

	case DNS_QTYPE_DLV:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unsupported resource record type"));
	  break;

#if ALIAS_ENABLED
	case DNS_QTYPE_ALIAS:
	  Warnx("%s: %s: %s", desctask(t), mydns_qtype_str(rr->type),
		_("unexpected resource record type - logic problem"));
	  break;
#endif

	}
      }
      break;
    }
  }
  return (0);
}
/*--- reply_process_rrlist() --------------------------------------------------------------------*/


/**************************************************************************************************
	TRUNCATE_RRLIST
	Returns new count of items in this list.
	The TC flag is _not_ set if data was truncated from the ADDITIONAL section.
**************************************************************************************************/
static int
truncate_rrlist(TASK *t, off_t maxpkt, RRLIST *rrlist, datasection_t ds) {
  register RR *rr = NULL;
  register int recs = 0;
#if DEBUG_ENABLED && DEBUG_REPLY
  int orig_recs = rrlist->size;
#endif

  /* Warn about truncated packets, but only if TCP is not enabled.  Most resolvers will try
     TCP if a UDP packet is truncated. */
  if (!tcp_enabled)
    Verbose("%s: %s", desctask(t), _("query truncated"));

  recs = rrlist->size;
  for (rr = rrlist->head; rr; rr = rr->next) {
    if ((off_t)(rr->offset + rr->length) >= maxpkt) {
      recs--;
      if (ds != ADDITIONAL)
	t->hdr.tc = 1;
    } else
      t->rdlen += rr->length;
  }
#if DEBUG_ENABLED && DEBUG_REPLY
  DebugX("reply", 1, _("%s section truncated from %d records to %d records"),
	 reply_datasection_str[ds], orig_recs, recs);
#endif
  return (recs);
}
/*--- truncate_rrlist() -------------------------------------------------------------------------*/


/**************************************************************************************************
	REPLY_CHECK_TRUNCATION
	If this reply would be truncated, removes any RR's that won't fit and sets the truncation flag.
**************************************************************************************************/
static void
reply_check_truncation(TASK *t, int *ancount, int *nscount, int *arcount) {
  size_t maxpkt = (t->protocol == SOCK_STREAM ? DNS_MAXPACKETLEN_TCP : DNS_MAXPACKETLEN_UDP);
  size_t maxrd = maxpkt - (DNS_HEADERSIZE + t->qdlen);

  if (t->rdlen <= maxrd)
    return;

#if DEBUG_ENABLED && DEBUG_REPLY
  DebugX("reply", 1, _("reply_check_truncation() needs to truncate reply (%u) to fit packet max (%u)"),
	 (unsigned int)t->rdlen, (unsigned int)maxrd);
#endif

  /* Loop through an/ns/ar sections, truncating as necessary, and updating counts */
  t->rdlen = 0;
  *ancount = truncate_rrlist(t, maxpkt, &t->an, ANSWER);
  *nscount = truncate_rrlist(t, maxpkt, &t->ns, AUTHORITY);
  *arcount = truncate_rrlist(t, maxpkt, &t->ar, ADDITIONAL);
}
/*--- reply_check_truncation() ------------------------------------------------------------------*/

void
abandon_reply(TASK *t) {
  /* Empty RR lists */
  rrlist_free(&t->an);
  rrlist_free(&t->ns);
  rrlist_free(&t->ar);

  /* Make sure reply is empty */
  t->replylen = 0;
  t->rdlen = 0;
  RELEASE(t->rdata);
}

/**************************************************************************************************
	BUILD_CACHE_REPLY
	Builds reply data from cached answer.
**************************************************************************************************/
void
build_cache_reply(TASK *t) {
  char *dest = t->reply;

  DNS_PUT16(dest, t->id);							/* Query ID */
  DNS_PUT(dest, &t->hdr, SIZE16);						/* Header */
}
/*--- build_cache_reply() -----------------------------------------------------------------------*/


/**************************************************************************************************
	BUILD_REPLY
	Given a task, constructs the reply data.
**************************************************************************************************/
void
build_reply(TASK *t, int want_additional) {
  char	*dest = NULL;
  int	ancount = 0, nscount = 0, arcount = 0;

  /* Add data to ADDITIONAL section */
  if (want_additional) {
    reply_add_additional(t, &t->an);
    reply_add_additional(t, &t->ns);
  }

  /* Sort records where necessary */
  if (t->an.a_records > 1)			/* ANSWER section: Sort A/AAAA records */
    sort_a_recs(t, &t->an, ANSWER);
  if (t->an.mx_records > 1)			/* ANSWER section: Sort MX records */
    sort_mx_recs(t, &t->an, ANSWER);
  if (t->an.srv_records > 1)			/* ANSWER section: Sort SRV records */
    sort_srv_recs(t, &t->an, ANSWER);
  if (t->ar.a_records > 1)			/* AUTHORITY section: Sort A/AAAA records */
    sort_a_recs(t, &t->ar, AUTHORITY);

  /* Build `rdata' containing resource records in ANSWER, AUTHORITY, and ADDITIONAL */
  t->replylen = DNS_HEADERSIZE + t->qdlen + t->rdlen;
  if (reply_process_rrlist(t, &t->an)
      || reply_process_rrlist(t, &t->ns)
      || reply_process_rrlist(t, &t->ar)) {
    abandon_reply(t);
  }

  ancount = t->an.size;
  nscount = t->ns.size;
  arcount = t->ar.size;

  /* Verify reply length */
  reply_check_truncation(t, &ancount, &nscount, &arcount);

  /* Make sure header bits are set correctly */
  t->hdr.qr = 1;
  t->hdr.cd = 0;

  /* Construct the reply */
  t->replylen = DNS_HEADERSIZE + t->qdlen + t->rdlen;
  dest = t->reply = ALLOCATE(t->replylen, char[]);

  DNS_PUT16(dest, t->id);					/* Query ID */
  DNS_PUT(dest, &t->hdr, SIZE16);				/* Header */
  DNS_PUT16(dest, t->qdcount);					/* QUESTION count */
  DNS_PUT16(dest, ancount);					/* ANSWER count */
  DNS_PUT16(dest, nscount);					/* AUTHORITY count */
  DNS_PUT16(dest, arcount);					/* ADDITIONAL count */
  if (t->qdlen && t->qd)
    DNS_PUT(dest, t->qd, t->qdlen);				/* Data for QUESTION section */
  DNS_PUT(dest, t->rdata, t->rdlen);				/* Resource record data */

#if DEBUG_ENABLED && DEBUG_REPLY
  DebugX("reply", 1, _("%s: reply:     id = %u"), desctask(t),
	 t->id);
  DebugX("reply", 1, _("%s: reply:     qr = %u (message is a %s)"), desctask(t),
	 t->hdr.qr, t->hdr.qr ? "response" : "query");
  DebugX("reply", 1, _("%s: reply: opcode = %u (%s)"), desctask(t),
	 t->hdr.opcode, mydns_opcode_str(t->hdr.opcode));
  DebugX("reply", 1, _("%s: reply:     aa = %u (answer %s)"), desctask(t),
	 t->hdr.aa, t->hdr.aa ? "is authoritative" : "not authoritative");
  DebugX("reply", 1, _("%s: reply:     tc = %u (message %s)"), desctask(t),
	 t->hdr.tc, t->hdr.tc ? "truncated" : "not truncated");
  DebugX("reply", 1, _("%s: reply:     rd = %u (%s)"), desctask(t),
	 t->hdr.rd, t->hdr.rd ? "recursion desired" : "no recursion");
  DebugX("reply", 1, _("%s: reply:     ra = %u (recursion %s)"), desctask(t),
	 t->hdr.ra, t->hdr.ra ? "available" : "unavailable");
  DebugX("reply", 1, _("%s: reply:  rcode = %u (%s)"), desctask(t),
	 t->hdr.rcode, mydns_rcode_str(t->hdr.rcode));
  /* escdata(t->reply, t->replylen); */
#endif
}
/*--- build_reply() -----------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
