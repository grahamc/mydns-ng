/**************************************************************************************************
	$Id: import-axfr.c,v 1.19 2005/04/20 17:22:25 bboy Exp $

	import-axfr.c: Import DNS data via AXFR.

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
#include <netdb.h>

static char *thishostname, *zone;			/* Hostname of remote host and zone */
static uchar *origin = NULL;				/* The origin name reported by the peer */
static uint32_t got_soa = 0;				/* Have we read the initial SOA record? */

extern int opt_notrim;					/* Don't remove trailing origin */
extern int opt_output;					/* Output instead of insert */

/**************************************************************************************************
	AXFR_CONNECT
	Connects to the remote server specified by `arg' (format "HOST[:PORT]/ZONE") and return a
	fd to newly created socket.
**************************************************************************************************/
static int
axfr_connect(char *hostportp, char **hostnamep) {
  char		hostport[512];
  char		*rem_hostname, *portp;
  unsigned int port = 53;
  int		fd, n;
  struct hostent	*he;
  struct sockaddr_in sa;

  strncpy(hostport, hostportp, sizeof(hostport)-1);

  if (!(rem_hostname = hostport) || !*rem_hostname)	/* Parse hostname, port, zone */
    Errx(_("host not specified"));
  if ((portp = strchr(hostport, ':')))
    *portp++ = '\0', port = atoi(portp);
  strtrim(zone);

  /* REMOVE any trailing dot(s) from end of zone name.. We automatically append the dot in
     request_axfr */
  while (LASTCHAR(zone) == '.')
    LASTCHAR(zone) = '\0';

  if (strlen(zone) > 256)
    Errx(_("zone too long"));

  *hostnamep = rem_hostname;

  Verbose(_("importing `%s' from %s:%u"), zone, rem_hostname, port);

  memset(&sa, 0, sizeof(struct sockaddr_in));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  if (!(he = gethostbyname(rem_hostname)))
    Errx("%s: %s", rem_hostname, _("unknown host"));

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    Err("%s", rem_hostname);

  for (n = 0; he->h_addr_list[n]; n++) {
    memcpy(&sa.sin_addr, he->h_addr_list[n], sizeof(struct in_addr));

    if (!connect(fd, (const struct sockaddr *)&sa, sizeof(struct sockaddr_in)))
      return (fd);

    Warn("%s (%s)", rem_hostname, inet_ntoa(sa.sin_addr));
  }
  close(fd);
  return (-1);
}
/*--- axfr_connect() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	MAKE_QUESTION
	Creates a question.  Returns the packet and stores the length of the packet in `packetlen'.
	The packet is dynamically allocated and should be free()'d.
**************************************************************************************************/
static char *
make_question(uint16_t id, dns_qtype_t qtype, char *name, size_t *packetlen) {
  char req[1024], *dest = req, *c;
  DNS_HEADER	header;
  size_t len;

  if (packetlen) *packetlen = 0;

  memset(&header, 0, sizeof(DNS_HEADER));
  DNS_PUT16(dest, id);						/* ID */
  header.rd = 1;
  memcpy(dest, &header, sizeof(DNS_HEADER)); dest += SIZE16;
  DNS_PUT16(dest, 1);						/* QDCOUNT */
  DNS_PUT16(dest, 0);						/* ANCOUNT */
  DNS_PUT16(dest, 0);						/* NSCOUNT */
  DNS_PUT16(dest, 0);						/* ARCOUNT */
  for (c = name; *c; c++)					/* QNAME */
    if (c == name || *c == '.') {
      char *end;
      if (c != name)
	c++;
      if ((end = strchr(c, '.')))
	*end = '\0';
      if ((len = strlen(c))) {
	if (len > 64) {
	  Warnx(_("zone contains invalid label (64 chars max)"));
	  return (NULL);
	}
	*dest++ = len;
	DNS_PUT(dest, c, len);
      }
      if (end)
	*end = '.';
    }
  *dest++ = 0;
  DNS_PUT16(dest, (uint16_t)qtype);				/* QTYPE */
  DNS_PUT16(dest, DNS_CLASS_IN);				/* QCLASS */
  len = dest - req;
  
  if (packetlen) *packetlen = len;

  c = ALLOCATE(len, char[]);
  memcpy(c, &req, len);

  return (c);
}
/*--- make_question() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	REQUEST_AXFR
	Constructs and sends the AXFR request packet.
**************************************************************************************************/
static void
request_axfr(int fd, char *rem_hostname, char *local_zone) {
  char		*qb, *q, *p;
  size_t	qlen;
  int		rv;
  size_t	off = 0;

  if (!(qb = make_question(getpid(), DNS_QTYPE_AXFR, local_zone, &qlen)))
    exit(EXIT_FAILURE);
  p = q = ALLOCATE(qlen + SIZE16, char[]);
  DNS_PUT16(p, qlen);
  memcpy(p, qb, qlen);
  RELEASE(qb);
  qlen += SIZE16;
  do {
    if ((rv = write(fd, q + off, qlen - off)) < 0)
      Err("%s: write", rem_hostname);
    off += rv;
  } while (off < qlen);
  RELEASE(q);
}
/*--- request_axfr() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	PROCESS_AXFR_SOA
	Find the SOA.  Insert it, and return the SOA record.
**************************************************************************************************/
static void
process_axfr_soa(uchar *name, uchar *reply, size_t replylen, uchar *src, uint32_t ttl) {
  uchar *ns, *mbox;
  task_error_t errcode;
  uint32_t serial, refresh, retry, expire, minimum;

  if (got_soa)
    return;

  if (!(ns = name_unencode2(reply, replylen, &src, &errcode)))
    Errx("%s SOA: %s: %s", name , _("error reading ns from SOA"), name);
  if (!(mbox = name_unencode2(reply, replylen, &src, &errcode)))
    Errx("%s SOA: %s: %s", name, _("error reading mbox from SOA"), name);
  DNS_GET32(serial, src);
  DNS_GET32(refresh, src);
  DNS_GET32(retry, src);
  DNS_GET32(expire, src);
  DNS_GET32(minimum, src);
  if (ttl < minimum)
    ttl = minimum;
  if (origin) RELEASE(origin);
  origin = (uchar*)STRDUP((char*)name);
  got_soa = import_soa(origin, ns, mbox, serial, refresh, retry, expire, minimum, ttl);
  RELEASE(ns);
  RELEASE(mbox);
}
/*--- process_axfr_soa() ------------------------------------------------------------------------*/


/**************************************************************************************************
	SHORTNAME
	Removes the origin from a name if it is present.
**************************************************************************************************/
static uchar *
shortname(uchar *name, int empty_name_is_ok) {
  size_t nlen = strlen((char*)name), olen = strlen((char*)origin);

  if (opt_notrim)
    return (name);
  if (nlen < olen)
    return (name);
  if (!strcasecmp((char*)origin, (char*)name)) {
    if (empty_name_is_ok)
      return ((uchar*)"");
    else
      return (name);
  }
  if (!strcasecmp((char*)(name + nlen - olen), (char*)origin))
    name[nlen - olen - 1] = '\0';
  return (name);
}
/*--- shortname() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	PROCESS_AXFR_ANSWER
	Processes a single answer.  If it's a SOA record, it is inserted, loaded, and the SOA record
	is returned.
**************************************************************************************************/
static uchar *
process_axfr_answer(uchar *reply, size_t replylen, uchar *src) {
  uchar *name, *data, *rv;
  task_error_t errcode;
  uint16_t type, class, rdlen;
  uint32_t ttl;

  if (!(name = name_unencode2(reply, replylen, &src, &errcode)))
    Errx("%s: %s: %s", thishostname, _("error reading name from answer section"), name);

  DNS_GET16(type, src);
  DNS_GET16(class, src);
  DNS_GET32(ttl, src);
  DNS_GET16(rdlen, src);
  rv = src + rdlen;

  if (!got_soa && type != DNS_QTYPE_SOA)
    Errx(_("got non-SOA RR before SOA"));

  switch (type) {
  case DNS_QTYPE_SOA:
    if (got_soa)
      return (NULL);
    process_axfr_soa(name, reply, replylen, src, ttl);
    break;

  case DNS_QTYPE_A:
    {
      struct in_addr addr;
      memcpy(&addr.s_addr, src, SIZE32);
      data = (uchar*)ipaddr(AF_INET, &addr);
      import_rr(shortname(name, 1), (uchar*)"A", data, strlen((char*)data), 0, ttl);
    }
    break;

  case DNS_QTYPE_AAAA:
    {
      uint8_t addr[16]; /* This is a cheat.
			** it should be a 'struct in6_addr'
			** but we can't be sure it will exist */

      memcpy(&addr, src, sizeof(addr));
      if ((data = (uchar*)ipaddr(AF_INET6, &addr))) {
	import_rr(shortname(name, 1), (uchar*)"AAAA", data, strlen((char*)data), 0, ttl);
      } else
	Notice("%s IN AAAA: %s", name, strerror(errno));
    }
    break;

  case DNS_QTYPE_CNAME:
    if (!(data = name_unencode2(reply, replylen, &src, &errcode)))
      Errx("%s CNAME: %s: %s", name, _("error reading data"), data);
    import_rr(shortname(name, 1), (uchar*)"CNAME", shortname(data, 0), strlen((char*)shortname(data, 0)), 0, ttl);
    RELEASE(data);
    break;

  case DNS_QTYPE_HINFO:
    {
      size_t len;
      int	quote1, quote2;
      uchar	*c, *data2;
      char	*insdata;

      len = *src++;
      data = ALLOCATE(len+1, char[]);
      memcpy(data, src, len);
      data[len] = '\0';
      src += len;
      for (c = data, quote1 = 0; *c; c++)
	if (!isalnum(*c))
	  quote1++;

      len = *src++;
      data2 = ALLOCATE(len+1, char[]);
      memcpy(data2, src, len);
      data2[len] = '\0';
      src += len;
      for (c = data2, quote2 = 0; *c; c++)
	if (!isalnum(*c))
	  quote2++;

      ASPRINTF(&insdata, "%s%s%s %s%s%s",
	       quote1 ? "\"" : "", data, quote1 ? "\"" : "",
	       quote2 ? "\"" : "", data2, quote2 ? "\"" : "");
      RELEASE(data);
      RELEASE(data2);
      import_rr(shortname(name, 1), (uchar*)"HINFO", (uchar*)insdata, strlen(insdata), 0, ttl);
      RELEASE(insdata);
    }
    break;

  case DNS_QTYPE_MX:
    {
      uint16_t pref;
      DNS_GET16(pref, src);
      if (!(data = name_unencode2(reply, replylen, &src, &errcode)))
	Errx("%s MX: %s: %s", name, _("error reading data"), data);
      import_rr(shortname(name, 1), (uchar*)"MX", shortname(data, 0), strlen((char*)shortname(data, 0)), pref, ttl);
      RELEASE(data);
    }
    break;

  case DNS_QTYPE_NS:
    if (!(data = name_unencode2(reply, replylen, &src, &errcode)))
      Errx("%s NS: %s: %s", name, _("error reading data"), data);
    import_rr(shortname(name, 1), (uchar*)"NS", shortname(data, 0), strlen((char*)shortname(data, 0)), 0, ttl);
    RELEASE(data);
    break;

  case DNS_QTYPE_PTR:
    {
      struct in_addr addr;
      addr.s_addr = mydns_revstr_ip4(name);
      if (!(data = name_unencode2(reply, replylen, &src, &errcode)))
	Errx("%s PTR: %s: %s", name, _("error reading data"), data);
      import_rr(shortname(name, 1), (uchar*)"PTR", shortname(data, 0), strlen((char*)shortname(data, 0)), 0, ttl);
      RELEASE(data);
    }
    break;

  case DNS_QTYPE_RP:
    {
      uchar *txtref;
      char *insdata;
      
      /* Get mbox in 'data' */
      if (!(data = name_unencode2(reply, replylen, &src, &errcode)))
	Errx("%s RP: %s: %s", name, _("error reading mbox"), data);
      
      /* Get txt in 'txtref' */
      if (!(txtref = name_unencode2(reply, replylen, &src, &errcode)))
	Errx("%s RP: %s: %s", name, _("error reading txt"), txtref);
      
      /* Construct data to insert */
      ASPRINTF(&insdata, "%s %s", shortname(data, 0), shortname(txtref, 0));
      RELEASE(data);
      RELEASE(txtref);
      import_rr(shortname(name, 1), (uchar*)"RP", (uchar*)insdata, strlen(insdata), 0, ttl);
      RELEASE(insdata);
    }
    break;

  case DNS_QTYPE_SRV:
    {
      uint16_t priority, weight, port;
      char		 *databuf;
      
      DNS_GET16(priority, src);
      DNS_GET16(weight, src);
      DNS_GET16(port, src);
      if (!(data = name_unencode2(reply, replylen, &src, &errcode)))
	Errx("%s SRV: %s: %s", name, _("error reading data"), data);
      ASPRINTF(&databuf, "%u %u %s", weight, port, shortname(data, 0));
      RELEASE(data);
      import_rr(shortname(name, 1), (uchar*)"SRV", (uchar*)databuf, strlen(databuf), priority, ttl);
      RELEASE(databuf);
    }
    break;

  case DNS_QTYPE_TXT:
    {
      size_t len = *src++;
      
      data = ALLOCATE(len + 1, char[]);
      memcpy(data, src, len);
      data[len] = '\0';
      src += len;
      import_rr(shortname(name, 1), (uchar*)"TXT", data, len, 0, ttl);
      RELEASE(data);
    }
    break;

  default:
    Warnx("%s %s: %s", name, mydns_qtype_str(type), _("discarding unsupported RR type"));
    break;
  }
  RELEASE(name);
  return (rv);
}
/*--- process_axfr_answer() ---------------------------------------------------------------------*/


/**************************************************************************************************
	PROCESS_AXFR_REPLY
**************************************************************************************************/
static int
process_axfr_reply(uchar *reply, size_t replylen) {
  uchar *src = reply, *name;
  task_error_t errcode;
  uint16_t n, qdcount, ancount;
  DNS_HEADER	hdr;

  /* Read packet header */
  src += SIZE16;					/* ID */
  memcpy(&hdr, src, SIZE16); src += SIZE16;
  DNS_GET16(qdcount, src);
  DNS_GET16(ancount, src);
  src += SIZE16 * 2;
  if (hdr.rcode != DNS_RCODE_NOERROR)
    Errx("%s: %s: %s", thishostname, _("server responded to our request with error"),
	 mydns_rcode_str(hdr.rcode));

#if DEBUG_ENABLED
  Debug("%u byte REPLY: qr=%u opcode=%s aa=%u tc=%u rd=%u ra=%u z=%u rcode=%u qd=%u an=%u",
	(unsigned int)replylen, hdr.qr, mydns_opcode_str(hdr.opcode),
	hdr.aa, hdr.tc, hdr.rd, hdr.ra, hdr.z, hdr.rcode, qdcount, ancount);
#endif

  /* Read question section(s) */
  for (n = 0; n < qdcount; n++) {
    if (!(name = name_unencode2(reply, replylen, &src, &errcode)))
      Errx("%s: %s: %s", thishostname, _("error reading name from question section"), name);
    src += (SIZE16 * 2);
    RELEASE(name);
  }

  /* Process all RRs in the answer section */
  for (n = 0; n < ancount; n++)
    if (!(src = process_axfr_answer(reply, replylen, src)))
      return (-1);

  return (0);
}
/*--- process_axfr_reply() ----------------------------------------------------------------------*/


/**************************************************************************************************
	IMPORT_AXFR
**************************************************************************************************/
void
import_axfr(char *hostport, char *import_zone) {
  uchar *reply, len[2];
  int fd;
  ssize_t replylen;

#if DEBUG_ENABLED
  Debug("STARTING AXFR of \"%s\" from %s", import_zone, hostport);
#endif

  thishostname = zone = NULL;
  got_soa = 0;

  zone = import_zone;

  /* Connect to remote host */
  if ((fd = axfr_connect(hostport, &thishostname)) < 0)
    Errx("%s: %s", hostport, _("failed to connect"));
#if DEBUG_ENABLED
  Debug("connected to %s", hostport);
#endif

  /* Send AXFR request */
  request_axfr(fd, thishostname, zone);

  /* Read packets from server and process them */
  while (recv(fd, len, 2, MSG_WAITALL) == 2) {
    if ((replylen = ((len[0] << 8) | (len[1]))) < 12)
      Errx(_("message too short"));
    reply = ALLOCATE(replylen, char[]);
    if (recv(fd, reply, replylen, MSG_WAITALL) != replylen)
      Errx(_("short message from server"));
    if (process_axfr_reply(reply, replylen))
      break;
    RELEASE(reply);
  }
  close(fd);

#if DEBUG_ENABLED
  Debug("COMPLETED AXFR of \"%s\" from %s", import_zone, hostport);
#endif
}
/*--- import_axfr() -----------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
