/**************************************************************************************************
	$Id: notify.c,v 1.0 2007/09/04 10:00:57 howard Exp $

	Copyright (C) 2007 Howard Wilkinson <howard@cohtech.com>

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
#define	DEBUG_NOTIFY	1

#define DEBUG_NOTIFY_SQL 1

#define DOMAINPORT 53   /* Port for notification servers */

typedef struct _notify_data {
  char			*origin;	/* Zone origin */
  uint32_t		soa_id;		/* SOA id from DB we are processing */
  ARRAY			*slaves;	/* Slaves */
} NOTIFYDATA;

typedef struct _init_data {
  int			zonecount;	/* Number of zones still to process */
  int			lastzone;	/* Last zoneid read in */
  ARRAY			*zones;		/* Zones to process */
} INITDATA;

static int notify_tasks_running = 0;
static int notifyfd = -1;

#if HAVE_IPV6
static int notifyfd6 = -1;
static int notify_tasks_running6 = 0;
#endif

static void
notify_free(TASK *t, void *data) {
  /*
   * Release resources and then return.
   * The data structure itself will be freed by caller
   */
  NOTIFYDATA *notify = (NOTIFYDATA*)data;

  array_free(notify->slaves, 1);

  RELEASE(notify->origin);

  if (t->family == AF_INET)
    notify_tasks_running -= 1;
#if HAVE_IPV6
  else if (t->family == AF_INET6)
    notify_tasks_running -= 1;
#endif

}

typedef time_t (*NotifyAlgorithm)(TASK *, NOTIFYSLAVE *);
 
static time_t
_notify_linear(TASK *t, NOTIFYSLAVE *slave) {
  return (notify_timeout);
}

static time_t
_notify_exponential(TASK *t, NOTIFYSLAVE *slave) {
  time_t timeout = notify_timeout;
  int i;

  for (i = 1; i < slave->retries; i++)
    timeout += timeout;

  return (timeout);
}

static time_t
_notify_progressive(TASK *t, NOTIFYSLAVE *slave) {
  return (notify_timeout * (slave->retries+1));
}

static time_t
_notify_timeout(TASK *t, NOTIFYSLAVE *slave) {
  static NotifyAlgorithm _notify_algorithm = NULL;
  time_t timeout = 0;

  if (!_notify_algorithm) {
    if (!strcasecmp(notify_algorithm, "linear")) _notify_algorithm = _notify_linear;
    else if (!strcasecmp(notify_algorithm, "exponential")) _notify_algorithm = _notify_exponential;
    else if (!strcasecmp(notify_algorithm, "progressive")) _notify_algorithm = _notify_progressive;
    else _notify_algorithm = _notify_linear;
  }

  timeout = _notify_algorithm(t, slave);
  return (timeout);
}

taskexec_t
notify_write(TASK *t) {
  NOTIFYDATA	*notify = (NOTIFYDATA*)t->extension;
  int		i = 0;
  int		slavecount = 0;
  time_t	timeout = INT_MAX;
  char		*out = NULL;
  size_t 	reqlen = 0;

  slavecount = array_numobjects(notify->slaves);

#if DEBUG_ENABLED && DEBUG_NOTIFY
  DebugX("notify", 1, _("%s: DNS NOTIFY notify_write called with %d slaves"),
	 desctask(t), slavecount);
#endif

  /*
   * Build notify packet
   */
  if (!(out = dns_make_notify(t, t->id, DNS_QTYPE_SOA, notify->origin, 0, &reqlen)))
    return TASK_FAILED;

  for (i = 0; i < array_numobjects(notify->slaves); i++) {
    NOTIFYSLAVE *slave = (NOTIFYSLAVE*)array_fetch(notify->slaves, i);
  
    struct sockaddr *slaveaddr = (struct sockaddr*)&(slave->slaveaddr);

    int		rv = 0;
    int		slavelen = 0;
    char	*msg = NULL;
    int		port = 0;

    if (slaveaddr->sa_family == AF_INET) {
      slavelen = sizeof(struct sockaddr_in);
#if HAVE_IPV6
    } else if (slaveaddr->sa_family == AF_INET6) {
      slavelen = sizeof(struct sockaddr_in6);
#endif
    } else {
      Warn(_("DNS NOTIFY Unknown address family %d"), slaveaddr->sa_family);
      return TASK_FAILED;
    }

    if (slaveaddr->sa_family == AF_INET) {
      msg = STRDUP(ipaddr(AF_INET, (void*)&(((struct sockaddr_in*)slaveaddr)->sin_addr)));
      port = ntohs(((struct sockaddr_in*)slaveaddr)->sin_port);
#if HAVE_IPV6
    } else if (slaveaddr->sa_family == AF_INET6) {
      msg = STRDUP(ipaddr(AF_INET6, (void*)&(((struct sockaddr_in6*)slaveaddr)->sin6_addr)));
      port = ntohs(((struct sockaddr_in6*)slaveaddr)->sin6_port);
#endif
    }

#if DEBUG_ENABLED && DEBUG_NOTIFY
    DebugX("notify", 1, _("%s: DNS NOTIFY notify_write processing slave %s(%d)"),
	   desctask(t), msg, port);
#endif
 
    if (slave->replied) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
      DebugX("notify", 1, _("%s: DNS NOTIFY notify_write slave %s(%d) has already replied"),
	     desctask(t), msg, port);
#endif
      RELEASE(msg);
      slavecount--;
      continue; /* slave has replied so ignore her */
    }

    if (slave->retries > notify_retries) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
      DebugX("notify", 1, _("%s: DNS NOTIFY notify_write slave %s(%d) has has exceeded retry count"),
	     desctask(t), msg, port);
#endif
      RELEASE(msg);
      slavecount--;
      continue; /* slave has exceeded his retry count so ignore him */
    }

    if (slave->lastsent > (current_time - _notify_timeout(t, slave))) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
      DebugX("notify", 1, _("%s: DNS NOTIFY notify_write slave %s(%d) has not timed out yet"),
	     desctask(t), msg, port);
#endif
      RELEASE(msg);
      continue; /* slave has not timed out yet - try again later */
    }

    if ((rv = sendto(t->fd, out, reqlen, 0, slaveaddr, slavelen)) < 0) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
      DebugX("notify", 1, _("%s: DNS NOTIFY notify_write send to slave %s(%d) failed - %s(%d)"), desctask(t),
	     msg, port, strerror(errno), errno);
#endif
      if (
	  (errno == EINTR)
#ifdef EAGAIN
	  || (errno == EAGAIN)
#else
#ifdef EWOULDBLOCK
	  || (errno == EWOULDBLOCK)
#endif
#endif
	  ) {
	RELEASE(msg);
	continue; /* Try again later */
      }
      Warn(_("Notify Send to Slave %s(%d) failed with error %s(%d)"), msg, port, strerror(errno), errno);
      RELEASE(msg);
      continue;
    }
#if DEBUG_ENABLED && DEBUG_NOTIFY
    else {
      DebugX("notify", 1, _("%s: DNS NOTIFY notify_write sent notify to slave %s:%d"),
	     desctask(t), msg, port);
    }
#endif
	   
    slave->lastsent = current_time;
    slave->replied = 0;
    slave->retries += 1;
    if (timeout > (current_time + _notify_timeout(t, slave))) {
      timeout = current_time + _notify_timeout(t, slave);
    }
    RELEASE(msg);
  }

  RELEASE(out);

  if(slavecount) {
    t->timeout = timeout;
    t->status = NEED_NOTIFY_RETRY;
  }

#if DEBUG_ENABLED && DEBUG_NOTIFY
  DebugX("notify", 1,
	 _("%s: DNS NOTIFY notify_write wrote notifies to slaves, %d left to reply, timeout %ld - %ld = %ld"),
	 desctask(t), slavecount, t->timeout, current_time, t->timeout - current_time);
#endif

  return ((slavecount > 0)?TASK_CONTINUE:TASK_COMPLETED);
}

/* Record the reply from the slave */
static int
_notify_read(TASK *t, struct sockaddr *from, socklen_t fromlen) {
  char		*msg = NULL;

  NOTIFYDATA	*notify = (NOTIFYDATA*)t->extension;
  int		i = 0;
  int		slavecount = 0;

  if (from->sa_family != t->family) return 1; /* Ignore this ... */

  if (from->sa_family == AF_INET) {
    msg = STRDUP(ipaddr(AF_INET, (void*)(&((struct sockaddr_in*)from)->sin_addr)));
#if HAVE_IPV6
  } else if (from->sa_family == AF_INET6) {
    msg = STRDUP(ipaddr(AF_INET6, (void*)(&((struct sockaddr_in6*)from)->sin6_addr)));
#endif
  }

#if DEBUG_ENABLED && DEBUG_NOTIFY
  DebugX("notify", 1, _("%s: DNS NOTIFY _notify_read read response from slave %s"), desctask(t), msg);
#endif

  slavecount = array_numobjects(notify->slaves);

  for (i = 0; i < array_numobjects(notify->slaves); i++) {
    NOTIFYSLAVE *slave = array_fetch(notify->slaves, i);

    struct sockaddr *slaveaddr = (struct sockaddr*)&(slave->slaveaddr);
    /*
     * Check that the message has been sent by the current slave, if not try next one!
     */
    if (from->sa_family == AF_INET) {
      if (memcmp(&((struct sockaddr_in*)from)->sin_addr,
		 &((struct sockaddr_in*)slaveaddr)->sin_addr,
		 sizeof(struct in_addr)) != 0) {
	if (slave->replied) slavecount--;
	continue;
      }
#if HAVE_IPV6
    } else if (from->sa_family == AF_INET6) {
      if (memcmp(&((struct sockaddr_in6*)from)->sin6_addr,
		 &((struct sockaddr_in6*)slaveaddr)->sin6_addr,
		 sizeof(struct in6_addr) != 0)) {
	if (slave->replied) slavecount--;
	continue;
      }
#endif
    }

    if (slave->replied) {
      Verbose(_("%s: DNS NOTIFY Duplicate reply from %s"), desctask(t), msg);
    } else {
      /* Mark this slave as finished */
      slave->replied = 1;
    }

    slavecount--;
  }

  RELEASE(msg);

  return slavecount;
}

taskexec_t
notify_read(TASK *t) {
  int			rv;

  do {
    char		*msg = NULL;
    uchar		in[DNS_MAXPACKETLEN_UDP];
    struct sockaddr	from;
    socklen_t 		fromlen = 0;
    TASK		*readT = NULL;
    DNS_HEADER		hdr;
    uint16_t		id = 0;
#if DEBUG_ENABLED && DEBUG_NOTIFY
    uint16_t		qdcount = 0, ancount = 0, nscount = 0, arcount = 0;
#endif
    uchar		*src = NULL;
    int			port = 0;

    memset(&from, 0, sizeof(from));
    memset(&hdr, 0, sizeof(hdr));

    src = in;

    fromlen = sizeof(from);
    rv = recvfrom(t->fd, (void*)in, sizeof(in), 0, &from, &fromlen);
    if (from.sa_family == AF_INET) {
      msg = STRDUP(ipaddr(AF_INET, (void*)&(((struct sockaddr_in*)&from)->sin_addr)));
      port = ntohs(((struct sockaddr_in*)&from)->sin_port);
#if HAVE_IPV6
    } else if (from.sa_family == AF_INET6) {
      msg = STRDUP(ipaddr(AF_INET6, (void*)&(((struct sockaddr_in6*)&from)->sin6_addr)));
      port = ntohs(((struct sockaddr_in6*)&from)->sin6_port);
#endif
    }

    if (rv < 0) {
      if (
	  (errno = EINTR)
#ifdef EAGAIN
	  || (errno == EAGAIN)
#else
#ifdef EWOULDBLOCK
	  || (errno == EWOULDBLOCK)
#endif
#endif
	  ) {
	goto CLEANUP;
      }
#if DEBUG_ENABLED && DEBUG_NOTIFY
      DebugX("notify", 1, _("%s: DNS NOTIFY notify_read recv from slave %s(%d) failed - %s(%d)"), desctask(t),
	     msg, port, strerror(errno), errno);
#endif
      Warn(_("recvfrom (UDP) for slave %s(%d) failed with %s(%d)"), msg, port, strerror(errno), errno);
      RELEASE(msg);
      goto CLEANUP;
    }

    if (rv < (int)DNS_HEADERSIZE) {
      Warn(_("recvfrom (UDP) for slave %s(%d) too short %dbytes should be > %d"), msg, port, rv, (int)DNS_HEADERSIZE);
      RELEASE(msg);
      continue;
    }

    DNS_GET16(id, src);
    memcpy(&hdr, src, SIZE16); src += SIZE16;

#if DEBUG_ENABLED && DEBUG_NOTIFY
    DNS_GET16(qdcount, src);
    DNS_GET16(ancount, src);
    DNS_GET16(nscount, src);
    DNS_GET16(arcount, src);
    DebugX("notify", 1, _("%s: DNS NOTIFY notify_read read response from slave %s length %d for id = %d, "
			  "qr = %d, opcode = %d, aa = %d, tc = %d, rd = %d, "
			  "ra = %d, z = %d, ad = %d, cd = %d, rcode = %d, "
			  "qdcount = %d, ancount = %d, nscount = %d, arcount = %d"),
	   desctask(t), msg, rv, id,
	   hdr.qr, hdr.opcode, hdr.aa, hdr.tc, hdr.rd,
	   hdr.ra, hdr.z, hdr.ad, hdr.cd, hdr.rcode,
	   qdcount, ancount, nscount, arcount);
#else
    src += (SIZE16*4);
#endif

    if (!hdr.qr || (hdr.opcode != DNS_OPCODE_NOTIFY)) {
      TASK *newt = NULL;
#if DEBUG_ENABLED && DEBUG_NOTIFY
      DebugX("notify", 1, _("%s: DNS NOTIFY notify_read got a non-notify response from %s"), desctask(t), msg);
      if ((hdr.opcode == DNS_OPCODE_QUERY) && !hdr.qr) {
	uchar *current = src;
	uchar *msg2 = NULL;
	task_error_t errcode = TASK_FAILED;
	DebugX("notify", 1, _("%s: DNS NOTIFY notify_read response is a query"), desctask(t));
	msg2 = name_unencode2(src, rv - DNS_HEADERSIZE, &current, &errcode);
	if(msg2) {
	  dns_qtype_t qtype = 0;
	  dns_class_t qclass = 0;
	  if (current + SIZE16 <= &in[rv]) {
	    DNS_GET16(qtype, current);
	    if (current+SIZE16 <= &in[rv]) {
	      DNS_GET16(qclass, current);
	      DebugX("notify", 1, _("%s: DNS NOTIFY notify_read %s asks for %s - %s against %s"), desctask(t),
		     msg, mydns_qtype_str(qtype), mydns_class_str(qclass), msg2);
	      RELEASE(msg2);
	      goto DECODEDQUERY;
	    }
	  }
	  RELEASE(msg2);
	}
	DebugX("notify", 1, _("%s: DNS NOTIFY notify_read %s sent malformed query - ignored"), desctask(t), msg);
      DECODEDQUERY: ;
      }
#endif
      Warn(_("message from %s not a query response or for wrong opcode"), msg);
      if ((newt = task_init(HIGH_PRIORITY_TASK, NEED_ANSWER, t->fd, SOCK_DGRAM, from.sa_family, &from))) {
	task_new(newt, (unsigned char*)in, rv);
      }
      RELEASE(msg);
      continue;
    }

    /*
     * Locate the actual task for this reply
     */
    readT = task_find_by_id(t, TaskArray[PERIODIC_TASK][NORMAL_PRIORITY_TASK], id);

    if (!readT) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
      DebugX("notify", 1, _("%s: DNS NOTIFY notify_read no task found for id %d from %s"), desctask(t), id, msg);
#endif
      RELEASE(msg);
      continue;
    }

    rv = _notify_read(readT, &from, fromlen);

#if DEBUG_ENABLED && DEBUG_NOTIFY
    DebugX("notify", 1, _("%s: DNS NOTIFY notify_read %d slaves still to be matched"), desctask(readT), rv);
#endif
    if (rv <= 0) {
      /* Task has finished */
      dequeue(readT);
    }
    {
      struct pollfd item;

      item.fd = t->fd;
      item.events = POLLIN;
      item.revents = 0;

#if HAVE_POLL
      rv = poll(&item, 1, 0);
      if ((rv >= 0) && !(item.revents & POLLIN)) { break; }
#else
#if HAVE_SELECT
      fd_set rfds, efds;
      struct timeval timeout = { 0, 0 };
      FD_ZERO(&rfds);
      FD_SET(t->fd, &rfds);
      FD_ZERO(&efds);
      FD_SET(t->fd, &efds);
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;
      rv = select(t->fd+1, &rfds, NULL, &efds, &timeout);
      if (rv >= 0) {
	if (!FD_ISSET(t->fd, &rfds)) { break; }
	if (FD_ISSET(t->fd, &efds)) { item.revents = POLLERR; rv = 1; }
      }
#else
#error You must have either poll(preferred) or select to compile this code
#endif
#endif
      if (rv < 0) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
	DebugX("notify", 1, _("%s: DNS NOTIFY notify_read poll fd %d failed with %s(%d)"),
	       desctask(t), t->fd, strerror(errno), errno);
#endif
	break;
      }
      if ((item.revents & POLLERR)) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
	DebugX("notify", 1, _("%s: DNS NOTIFY notify_read poll fd %d gave an error"), desctask(t), t->fd);
#endif
	break;
      }
    }
    RELEASE(msg);
  } while(1);

 CLEANUP:
  if (t->family == AF_INET)
    rv = notify_tasks_running;
#if HAVE_IPV6
  else if (t->family == AF_INET6)
    rv = notify_tasks_running6;
#endif

  return ((rv > 0)?TASK_CONTINUE:TASK_COMPLETED);
}

static TASK *
notify_running(TASK *t, MYDNS_SOA *soa) {
  TASK *checkt = NULL;
  int j = 0;
  /*
   * Scan running tasks for one that is doing notification on this soa
   */

  for (j = HIGH_PRIORITY_TASK; j <= LOW_PRIORITY_TASK; j++) {
    for (checkt = TaskArray[PERIODIC_TASK][j]->head; checkt; checkt = checkt->next) {
      if (t->freeextension == notify_free) {
	NOTIFYDATA *notify = t->extension;
	if (notify->soa_id == soa->id) {
	  return checkt;
	}
      }
    }
  }
  return NULL;
}

static ARRAY *
notify_get_server_list(TASK *t, MYDNS_SOA *soa)
{
  /*
   * Get the NS records from the DB that match the origin in the soa
   * Retrieve the also-notify field from the SOA
   * Merge the 2 lists removing duplicates
   * Remove the Master Server from the List
   */
  ARRAY		*name_servers = array_init(4);

  MYDNS_RR	*rr = NULL, *r = NULL;

  SQL_RES	*res = NULL;
  SQL_ROW	row = NULL;

  size_t	querylen = 0;
  char 		*query = NULL;

  rr = find_rr(t, soa, DNS_QTYPE_NS, "");

  for (r = rr; r; r = r->next) {
    char *name_server;
    /* Check name_server is absolute, if not add origin */
    if (!MYDNS_RR_DATA_LENGTH(r)) continue;
    name_server = mydns_rr_append_origin(MYDNS_RR_DATA_VALUE(r), soa->origin);
    if (name_server == MYDNS_RR_DATA_VALUE(r)) name_server = STRDUP(name_server);
    if (!strcasecmp(name_server, soa->ns)) {
      /* Filter out Master as that is us! */
      RELEASE(name_server);
      continue;
    }
    array_append(name_servers, name_server);
  }

  mydns_rr_free(rr);

  if (sql_iscolumn(sql, mydns_soa_table_name, "also_notify")) {
    /* Retrieve the ALSO NOTIFY Entries from the SOA */
    querylen = sql_build_query(&query, "SELECT also_notify FROM %s WHERE id=%u;",
			       mydns_soa_table_name, soa->id);
#if DEBUG_ENABLED && DEBUG_NOTIFY_SQL
    DebugX("notify-sql", 1, _("%s: DNS NOTIFY: notify_get_server_list %s"), desctask(t), query);
#endif
    res = sql_query(sql, query, querylen);
    RELEASE(query);
    if (res) {
      if ((row = sql_getrow(res, NULL))) {
	char *also_notify_servers = row[0];
	char *also_notify_server = NULL;
	uint scanned = 0;

	if (also_notify_servers) {
	  while (scanned < strlen(also_notify_servers)) {
	    char *start = &also_notify_servers[scanned];
	    char *comma = strchr(start, ',');
	    int i;

	    if (comma) {
	      also_notify_server = ALLOCATE(comma - start + 1, char[]);
	      strncpy(also_notify_server, start, comma - start);
	      also_notify_server[comma - start] = '\0';
	      scanned += comma - start + 1;
	    } else {
	      also_notify_server = ALLOCATE(strlen(start)+1, char[]);
	      strncpy(also_notify_server, start, strlen(start));
	      also_notify_server[strlen(start)] = '\0';
	      scanned += strlen(start) + 1;
	    }
	    for (i = 0; i < array_numobjects(name_servers); i++) {
	      char *nsn = (char*)array_fetch(name_servers, i);
	      if (!nsn) continue;
	      if (!strcasecmp(also_notify_server, nsn)) {
		RELEASE(also_notify_server);
		goto NEXTSERVER;
	      }
	    }
	    array_append(name_servers, also_notify_server);
	  NEXTSERVER: continue;
	  }
	}
      }
      sql_free(res);
    } else {
      WarnSQL(sql, _("error loading DNS NOTIFY also_notify nameservers: %s"), desctask(t));
    }
  }

#if DEBUG_ENABLED && DEBUG_NOTIFY
  DebugX("notify", 1,
	 _("%s: DNS NOTIFY notify_get_server_list found %d name servers in zone data and also-notify"),
	 desctask(t), array_numobjects(name_servers));
#endif

  return name_servers;
}

int
name_servers2ip(TASK *t, ARRAY *name_servers,
		ARRAY *ips4,
		ARRAY *ips6
		) {
  ARRAY *ipaddresses = array_init(16);
  int i = 0;

#if DEBUG_ENABLED
  DebugX("notify", 1, _("%s: name_servers2ip() called name_servers = %p"),
	 desctask(t), name_servers);
#endif

  if(!name_servers) return 0;

#if DEBUG_ENABLED
  DebugX("notify", 1, _("%s: name_servers2ip() called name_servers count = %d"),
	 desctask(t), array_numobjects(name_servers));
#endif

  for (i = 0; i < array_numobjects(name_servers); i++) {
    char	*name_server = array_fetch(name_servers, i);
    char	*label = NULL;
    int		resolved = 0;

    MYDNS_SOA *nsoa = NULL;
    MYDNS_RR *rr = NULL;

#if DEBUG_ENABLED
    DebugX("notify", 1, _("%s: name_servers2ip() processing name server %s"),
	   desctask(t), name_server);
#endif

    nsoa = find_soa2(t, name_server, &label);

    if (nsoa) {
      MYDNS_RR *r = NULL;

#if DEBUG_ENABLED
      DebugX("notify", 1, _("%s: name_servers2ip() processing rr %s for name server %s"),
	     desctask(t), label, name_server);
#endif

      rr = find_rr(t, nsoa, DNS_QTYPE_A, label);

      for (r = rr; r; r = r->next) {
	int j = 0;
	struct sockaddr_in *ipaddr4 = NULL;
	NOTIFYSLAVE *slave = NULL;
	resolved++;
	for (j = 0; j < array_numobjects(ipaddresses); j++) {
	  char *ipa = (char*)array_fetch(ipaddresses, j);
	  if (!ipa) continue;
	  if (strlen(ipa) != MYDNS_RR_DATA_LENGTH(r)) goto NextA_RR;
	  if (!strncmp(MYDNS_RR_DATA_VALUE(r), ipa, MYDNS_RR_DATA_LENGTH(r))) goto NextA_RR;
	}
#if DEBUG_ENABLED
	DebugX("notify", 1, _("%s: name_servers2ip() processing name server %s got ip address %s(%d)"),
	       desctask(t), name_server, (char*)MYDNS_RR_DATA_VALUE(r), MYDNS_RR_DATA_LENGTH(r));
#endif
	slave = (NOTIFYSLAVE*)ALLOCATE(sizeof(NOTIFYSLAVE), NOTIFYSLAVE);
	slave->lastsent = 0;
	slave->replied = 0;
	slave->retries = 0;
	ipaddr4 = (struct sockaddr_in*)&(slave->slaveaddr);
	ipaddr4->sin_family = AF_INET;
	ipaddr4->sin_port = htons(DOMAINPORT);
	if ((j = inet_pton(AF_INET, MYDNS_RR_DATA_VALUE(r), (void*)&(ipaddr4->sin_addr))) < 0) {
	  Warn(_("inet_pton - failed with %s(%d)"), strerror(errno), errno);
	  RELEASE(slave);
	  continue;
	} else if (j == 0) {
	  Warn(_("inet_pton - failed to convert ip address %s"), (char*)MYDNS_RR_DATA_VALUE(r));
	  RELEASE(slave);
	  continue;
	}
#if DEBUG_ENABLED
	DebugX("notify", 1, _("%s: name_servers2ip() processing name server %s append IP address to saved vector"),
	       desctask(t), name_server);
#endif
	array_append(ipaddresses, STRNDUP(MYDNS_RR_DATA_VALUE(r), MYDNS_RR_DATA_LENGTH(r)));
	array_append(ips4, slave);

      NextA_RR: continue;
      }

      mydns_rr_free(rr);

#if HAVE_IPV6
      rr = find_rr(t, nsoa, DNS_QTYPE_AAAA, label);

      for (r = rr; r; r = r->next) {
	int j = 0;
	struct sockaddr_in6 *ipaddr6 = NULL;
	NOTIFYSLAVE *slave = NULL;
	resolved++;
	for (j = 0; j < array_numobjects(ipaddresses); j++) {
	  char *ipa = (char*)array_fetch(ipaddresses, j);
	  if (!ipa) continue;
	  if (strlen(ipa) != MYDNS_RR_DATA_LENGTH(r)) goto NextAAAA_RR;
	  if (!strncmp(MYDNS_RR_DATA_VALUE(r), ipa, MYDNS_RR_DATA_LENGTH(r))) goto NextAAAA_RR;
	}
#if DEBUG_ENABLED
	DebugX("notify", 1, _("%s: name_servers2ip() processing name server %s got ip address %s(%d)"),
	       desctask(t), name_server, (char*)MYDNS_RR_DATA_VALUE(r), MYDNS_RR_DATA_LENGTH(r));
#endif
	slave = (NOTIFYSLAVE*)ALLOCATE(sizeof(NOTIFYSLAVE), NOTIFYSLAVE);
	slave->lastsent = 0;
	slave->replied = 0;
	slave->retries = 0;
	ipaddr6 = (struct sockaddr_in6*)&(slave->slaveaddr);
	ipaddr6->sin6_family = AF_INET6;
	ipaddr6->sin6_port = htons(DOMAINPORT);
	if ((j = inet_pton(AF_INET6, MYDNS_RR_DATA_VALUE(r), (void*)&(ipaddr6->sin6_addr))) < 0) {
	  Warn(_("inet_pton - failed with %s(%d)"), strerror(errno), errno);
	  RELEASE(slave);
	  continue;
	} else if (j == 0) {
	  Warn(_("inet_pton - failed to convert ip address %s"), (char *)MYDNS_RR_DATA_VALUE(r));
	  RELEASE(slave);
	  continue;
	}
#if DEBUG_ENABLED
	DebugX("notify", 1, _("%s: name_servers2ip() processing name server %s append IP address to saved vector"),
	       desctask(t), name_server);
#endif
	array_append(ipaddresses, STRNDUP(MYDNS_RR_DATA_VALUE(r), MYDNS_RR_DATA_LENGTH(r)));
	array_append(ips6, slave);

      NextAAAA_RR: continue;
      }

      mydns_rr_free(rr);

#endif
      mydns_soa_free(nsoa);

    }

    RELEASE(label);

    if (resolved) { continue; }

#if DEBUG_ENABLED
    DebugX("notify", 1, _("%s: name_servers2ip() - done try local DNS - %d"), desctask(t), resolved);
#endif

    /* This needs enhancing so that a recursive lookup is run before trying the local DNS */

    /* Look up via other DNS/hosts instead */
#if HAVE_GETADDRINFO
    {
      struct addrinfo hosthints;
      struct addrinfo *hostdata = NULL;
      struct addrinfo *hostentry;
      int rv;

      hosthints.ai_flags = AI_CANONNAME|AI_ADDRCONFIG;
#if HAVE_IPV6
      hosthints.ai_family = AF_UNSPEC;
#else
      hosthints.ai_family = AF_INET;
#endif
      hosthints.ai_socktype = 0;
      hosthints.ai_protocol = 0;
      hosthints.ai_addrlen = 0;
      hosthints.ai_addr = NULL;
      hosthints.ai_canonname = NULL;
      hosthints.ai_next = NULL;

      rv = getaddrinfo(name_server, NULL, NULL, &hostdata);

      if (!rv) {
	int k;

	for (hostentry = hostdata; hostentry; hostentry = hostentry->ai_next) {
	  const char *ipaddress = NULL;
	  NOTIFYSLAVE *slave = (NOTIFYSLAVE*)ALLOCATE(sizeof(NOTIFYSLAVE), NOTIFYSLAVE);
	  slave->lastsent = 0;
	  slave->replied = 0;
	  slave->retries = 0;
	  if (hostentry->ai_family == AF_INET) {
	    struct sockaddr_in *ipaddr4 = (struct sockaddr_in*)&(slave->slaveaddr);
	    memcpy((void*)ipaddr4, (void*)hostentry->ai_addr, hostentry->ai_addrlen);
	    ipaddr4->sin_port = htons(DOMAINPORT);
	    ipaddress = ipaddr(AF_INET, (void*)&ipaddr4->sin_addr);
#if HAVE_IPV6
	  } else if (hostentry->ai_family == AF_INET6) {
	    struct sockaddr_in6 *ipaddr6 = (struct sockaddr_in6*)&(slave->slaveaddr);
	    memcpy((void*)ipaddr6, (void*)hostentry->ai_addr, hostentry->ai_addrlen);
	    ipaddr6->sin6_port = htons(DOMAINPORT);
	    ipaddress = ipaddr(AF_INET6, (void*)&ipaddr6->sin6_addr);
#endif
	  }

	  for (k = 0; k < array_numobjects(ipaddresses); k++) {
	    char *ipa = array_fetch(ipaddresses, k);
	    if (!ipa) continue;
	    if(!strcmp(ipaddress, ipa)) {
	      RELEASE(slave);
	      goto DONETHATONE;
	    }
	  }
	  array_append(ipaddresses, STRDUP(ipaddress));
	  if (hostentry->ai_family == AF_INET) {
	    array_append(ips4, slave);
#if HAVE_IPV6
	  } else if (hostentry->ai_family == AF_INET6) {
	    array_append(ips6, slave);
#endif
	  }
	DONETHATONE: continue;
	}
	freeaddrinfo(hostdata);
      }
#else
      struct hostent *hostdata = gethostbyname(name_server);

      if (hostdata) {
	int j, k;

	for (j = 0; hostdata->h_addr_list[j]; j++) {
	  const char *ipaddress = NULL;
	  NOTIFYSLAVE *slave = (NOTIFYSLAVE*)ALLOCATE(sizeof(NOTIFYSLAVE), NOTIFYSLAVE);
	  slave->lastsent = 0;
	  slave->replied = 0;
	  slave->retries = 0;
	  if (hostdata->h_addrtype == AF_INET) {
	    struct sockaddr_in *ipaddr4 = (struct sockaddr_in*)&(slave->slaveaddr);
	    ipaddr4->sin_family = AF_INET;
	    ipaddr4->sin_port = htons(DOMAINPORT);
	    memcpy((void*)&ipaddr4->sin_addr, (void*)hostdata->h_addr_list[j], hostdata->h_length);
	    ipaddress = ipaddr(AF_INET, (void*)&ipaddr4->sin_addr);
#if HAVE_IPV6
	  } else if (hostdata->h_addrtype == AF_INET6) {
	    struct sockaddr_in6 *ipaddr6 = (struct sockaddr_in6*)&(slave->slaveaddr);
	    ipaddr6->sin6_family = AF_INET6;
	    ipaddr6->sin6_port = htons(DOMAINPORT);
	    memcpy((void*)&ipaddr6->sin6_addr, (void*)hostdata->h_addr_list[j], hostdata->h_length);
	    ipaddress = ipaddr(AF_INET6, (void*)&ipaddr6->sin6_addr);
#endif
	  }

	  for (k = 0; k < array_numobjects(ipaddresses); k++) {
	    char *ipa = array_fetch(ipaddresses, k);
	    if (!ipa) continue;
	    if(!strcmp(ipaddress, ipa)) {
	      RELEASE(slave);
	      goto DONETHATONE;
	    }
	  }
	  array_append(ipaddresses, STRDUP(ipaddress));
	  if (hostdata->h_addrtype == AF_INET) {
	    array_append(ips4, slave);
#if HAVE_IPV6
	  } else if (hostdata->h_addrtype == AF_INET6) {
	    array_append(ips6, slave);
#endif
	  }
	DONETHATONE: continue;
	}
      }
#endif
    }

  }

#if DEBUG_ENABLED
  DebugX("notify", 1,
	 _("%s: name_servers2ip() - releasing data and then returning ipaddresses=%p, name_servers=%p"),
	 desctask(t), ipaddresses, name_servers);
#endif

  array_free(ipaddresses, 1);
  array_free(name_servers, 1);

#if DEBUG_ENABLED
  DebugX("notify", 1, _("%s: name_servers2ip() - returning"), desctask(t));
#endif

  return array_numobjects(ips4)
#if HAVE_IPV6
    + array_numobjects(ips6)
#endif
    ;
}

static int
notify_allocate_fd(int family, struct sockaddr *sourceaddr)
{
  int fd = -1;

  if ((fd = socket(family, SOCK_DGRAM, IPPROTO_UDP)) >= 0) {
    int n = 0, opt = 1;

    /* Set REUSEADDR in case anything is left lying around. */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
      Warn(_("setsockopt - %s(%d)"), strerror(errno), errno);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    if (bind(fd, sourceaddr,
#if HAVE_IPV6
	     (family == AF_INET6)?sizeof(struct sockaddr_in6):
#endif
	     sizeof(struct sockaddr_in)) < 0) {
      char		*msg = NULL;
      int		port = 0;
      if (family == AF_INET) {
	msg = STRDUP(ipaddr(AF_INET, (void*)&(((struct sockaddr_in*)sourceaddr)->sin_addr)));
	port = ntohs(((struct sockaddr_in*)sourceaddr)->sin_port);
#if HAVE_IPV6
      } else if (family == AF_INET6) {
	msg = STRDUP(ipaddr(AF_INET6, (void*)&(((struct sockaddr_in6*)sourceaddr)->sin6_addr)));
	port = ntohs(((struct sockaddr_in6*)sourceaddr)->sin6_port);
#endif
      }
      Warn(_("bind (UDP): %s:%d - %s(%d)"), msg, port, strerror(errno), errno);
      RELEASE(msg);
    }
    for (n = 1; n < 1024; n++) {
      opt = n * 1024;
      if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt)) < 0)
	  break;
    }
  }

  return fd;
}

static struct sockaddr *
notify_get_source(int family, const char *sourceaddr)
{
  struct sockaddr *res = NULL;

  if (family == AF_INET) {
    res = (struct sockaddr*)ALLOCATE(sizeof(struct sockaddr_in), struct sockaddr_in);
    ((struct sockaddr_in*)res)->sin_port = htons(0); /* Random port selection */
    inet_pton(family, sourceaddr, (void*)&(((struct sockaddr_in*)res)->sin_addr));
#if HAVE_IPV6
  } else if (family == AF_INET6) {
    res = (struct sockaddr*)ALLOCATE(sizeof(struct sockaddr_in6), struct sockaddr_in6);
    ((struct sockaddr_in6*)res)->sin6_port = htons(0); /* Random port selection */
    inet_pton(family, sourceaddr, (void*)&(((struct sockaddr_in6*)res)->sin6_addr));
#endif
  }

  return res;
}

static void
notify_master_free(TASK *t, void *data) {

#if DEBUG_ENABLED && DEBUG_NOTIFY
  DebugX("notify", 1, _("%s: DNS NOTIFY notify_master_free freeing fd = %d"), desctask(t), t->fd);
#endif
  sockclose(t->fd);

  if (t->family == AF_INET) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
    DebugX("notify", 1, _("%s: DNS NOTIFY notify_master_free notify fd = %d"), desctask(t), notifyfd);
#endif
    notifyfd = -1;
#if HAVE_IPV6
  } else if (t->family == AF_INET6) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
    DebugX("notify", 1, _("%s: DNS NOTIFY notify_master_free notify fd = %d"), desctask(t), notifyfd6);
#endif
    notifyfd6 = -1;
#endif
  }
}

static taskexec_t
notify_master_tick(TASK *t, int extension) {

  if (
      ((t->family == AF_INET) && (notify_tasks_running <= 0))
#if HAVE_IPV6
      || ((t->family == AF_INET6) && (notify_tasks_running6 <= 0))
#endif
      ) {
    return TASK_COMPLETED;
  }

  t->timeout = current_time + 3600;

  return TASK_CONTINUE;
}

static taskexec_t
notify_tick(TASK *t, void *extension) {
  t->status = NEED_NOTIFY_WRITE;

  return notify_write(t);
}

void
notify_slaves(TASK *t, MYDNS_SOA *soa) {
  TASK *notify_task = NULL, *notify_master = NULL;
  NOTIFYDATA *notify = NULL;

#if DEBUG_ENABLED && DEBUG_NOTIFY
  DebugX("notify", 1, _("%s: DNS NOTIFY notify_slaves called for %s"), desctask(t), soa->origin);
#endif

  /*
   * Locate a currently running NOTIFY task for this SOA
   */
  if ((notify_task = notify_running(t, soa))) {
    int		i;
    notify = (NOTIFYDATA*)t->extension;
    for (i = 0; i < array_numobjects(notify->slaves); i++) {
      NOTIFYSLAVE *slave = array_fetch(notify->slaves, i);
      slave->lastsent = 0;
      slave->replied = 0;
      slave->retries = 0;
    }
    t->timeout = current_time;
  } else {
    /*
     * Build a new task to process this notify operation
     *
     * Retrieve the list of nameservers for this zone from the rr table
     * Append any nameservers given in the also-notify column of the soa table
     * remove the master server from the list
     * remove any duplicates from the list
     * convert to IP addresses
     * remove any duplicate ip addresses from the list
     *
     * Allocate UDP port for this task
     *
     * Queue task for later running - we do not do anything yet as an update storm
     * could result in this task being pushed up a number of times
     */
    ARRAY *name_servers = notify_get_server_list(t, soa);
    ARRAY *slavesipv4 = array_init(8);
#if HAVE_IPV6
    ARRAY *slavesipv6 = array_init(8);
#endif
    int slavecount = name_servers2ip(t, name_servers, slavesipv4,
#if HAVE_IPV6
				     slavesipv6
#else
				     NULL
#endif
				     );

#if DEBUG_ENABLED && DEBUG_NOTIFY
    DebugX("notify", 1, _("%s: DNS NOTIFY notify_slaves has %d slaves to notify for %s"),
	   desctask(t), slavecount, soa->origin);
#endif

    if (slavecount == 0) {
      array_free(slavesipv4, 1);
#if HAVE_IPV6
      array_free(slavesipv6,1);
#endif
      return; /* No-one to inform */
    }

    if (array_numobjects(slavesipv4) > 0) {
      static struct sockaddr *notifysource4 = NULL;
      if (!notifysource4)
	notifysource4 = notify_get_source(AF_INET, conf_get(&Conf, "notify-source", NULL));
      /* Allocate an fd for this protocol */
      if (notifyfd < 0) {
	notifyfd = notify_allocate_fd(AF_INET, notifysource4);
	if (notifyfd < 0) {
	  /* Failed to get socket just ignore this operation */
	  array_free(slavesipv4, 1);
	  goto DONEIPV4;
	}
      }
#if DEBUG_ENABLED && DEBUG_NOTIFY
      DebugX("notify", 1, _("%s: DNS NOTIFY notify_slaves allocated fd = %d"), desctask(t), notifyfd);
#endif
      if (notify_tasks_running <= 0) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
	DebugX("notify", 1, _("%s: DNS NOTIFY notify_slaves initializing master IPV4 task for %s"),
	       desctask(t), soa->origin);
#endif
	notify_master = IOtask_init(NORMAL_PRIORITY_TASK, NEED_NOTIFY_READ, notifyfd,
				    SOCK_DGRAM, AF_INET, notifysource4);
	notify_master->timeout = current_time + 3600;
	task_add_extension(notify_master, NULL, (FreeExtension)notify_master_free, NULL,
			   (TimeExtension)notify_master_tick);
	notify_tasks_running = 0;
      }
      notify = (NOTIFYDATA*)ALLOCATE(sizeof(NOTIFYDATA), NOTIFYDATA);
      notify->origin = STRDUP(soa->origin);
      notify->soa_id = soa->id;
      notify->slaves = slavesipv4;

#if DEBUG_ENABLED && DEBUG_NOTIFY
	DebugX("notify", 1, _("%s: DNS NOTIFY notify_slaves initializing notifier IPV4 task for %s"),
	       desctask(t), soa->origin);
#endif
      notify_task = Ticktask_init(NORMAL_PRIORITY_TASK, NEED_NOTIFY_WRITE, notifyfd,
				  SOCK_DGRAM, AF_INET, NULL);
      notify_task->timeout = current_time; /* Timeout immediately and therefore run */
      notify_task->id = notify_task->internal_id;
      task_add_extension(notify_task,
			 (void*)notify,
			 (FreeExtension)notify_free,
			 NULL,
			 (TimeExtension)notify_tick);
      notify_tasks_running += 1;
    } else {
      array_free(slavesipv4, 0);
    }
  DONEIPV4:
#if HAVE_IPV6
    if (array_numobjects(slavesipv6)) {
      static struct sockaddr *notifysource6 = NULL;
      if (!notifysource6)
	notifysource6 = notify_get_source(AF_INET6,
					  conf_get(&Conf, "notify-source6", NULL));
      /* Allocate an fd for this protocol */
      if (notifyfd6 < 0) {
	notifyfd6 = notify_allocate_fd(AF_INET6, notifysource6);
	if (notifyfd6 < 0) {
	  /* Failed to get socket just ignore this operation */
	  array_free(slavesipv6, 1);
	  goto DONEIPV6;
	}
      }
#if DEBUG_ENABLED && DEBUG_NOTIFY
      DebugX("notify", 1, _("%s: DNS NOTIFY notify_slaves allocated fd = %d"), desctask(t), notifyfd6);
#endif
      if (notify_tasks_running6 <= 0) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
	DebugX("notify", 1, _("%s: DNS NOTIFY notify_slaves initializing master IPV6 task for %s"),
	       desctask(t), soa->origin);
#endif
	notify_master = IOtask_init(NORMAL_PRIORITY_TASK, NEED_NOTIFY_READ, notifyfd6,
				    SOCK_DGRAM, AF_INET6, notifysource6);
	notify_master->timeout = current_time + 3600;
	task_add_extension(notify_master, NULL, (FreeExtension)notify_master_free, NULL,
			   (TimeExtension)notify_master_tick);
	notify_tasks_running6 = 0;
      }
      notify = (NOTIFYDATA*)ALLOCATE(sizeof(NOTIFYDATA), NOTIFYDATA);
      notify->origin = STRDUP(soa->origin);
      notify->soa_id = soa->id;
      notify->slaves = slavesipv6;
      
#if DEBUG_ENABLED && DEBUG_NOTIFY
	DebugX("notify", 1, _("%s: DNS NOTIFY notify_slaves initializing notifier IPV6 task for %s"),
	       desctask(t), soa->origin);
#endif
      notify_task = Ticktask_init(NORMAL_PRIORITY_TASK, NEED_NOTIFY_WRITE, notifyfd,
				  SOCK_DGRAM, AF_INET6, NULL);
      notify_task->timeout = 0; /* Timeout immediately and therefore run */
      notify_task->id = notify_task->internal_id;
      task_add_extension(notify_task,
			 (void*)notify,
			 (FreeExtension)notify_free,
			 NULL,
			 (TimeExtension)notify_tick);
      notify_tasks_running6 += 1;
    } else {
      array_free(slavesipv6, 0);
    }
  DONEIPV6:
    ;
#endif
  }
  return;
}

static void
notify_initfree(TASK *t, void *data) {
  INITDATA	*initdata = (INITDATA*)data;

  if (initdata->zones != NULL) {
    array_free(initdata->zones, 1);
    initdata->zones = NULL;
  }
}

static taskexec_t
notify_all_soas(TASK *t, void *data) {
  INITDATA	*initdata = (INITDATA*)data;

  SQL_RES	*res = NULL;
  SQL_ROW	row = NULL;

  char *zone = NULL;
  MYDNS_SOA *soa;

  if (initdata->zones == NULL) {
    size_t	querylen = 0;
    char	*query = NULL;

    initdata->zones = array_init(initdata->zonecount);

    initdata->zonecount = 0;

    querylen = sql_build_query(&query, "SELECT origin,id from %s%s%s%s ORDER BY id ASC;",
			       mydns_soa_table_name,
			       (mydns_soa_use_active)? " WHERE active='" : "",
			       (mydns_soa_use_active)? mydns_soa_active_types[0] : "",
			       (mydns_soa_use_active)? "'" : "");

    /* Retrieve next SOA and build a task for it */
    res = sql_query(sql, query, querylen);
    RELEASE(query);
    if(!res) {
      WarnSQL(sql, _("error loading DNS NOTIFY zone origins while building all_soas: %s"), desctask(t));
      return TASK_FAILED;
    }

    while ((row = sql_getrow(res, NULL))) {
      char *origin = row[0];

#if DEBUG_ENABLED && DEBUG_NOTIFY
      DebugX("notify", 1, _("%s: DNS NOTIFY notify_all_soas prime zone %s for NOTIFY check"),
	     desctask(t), origin);
#endif
      array_append(initdata->zones, STRDUP(origin));
      initdata->zonecount += 1;
    }

    sql_free(res);
  }

  if(initdata->zonecount <= 0) return (TASK_COMPLETED);

  t->timeout = current_time + 1; /* Wait at least 1 seconds before firing the next one */

  zone = array_fetch(initdata->zones, initdata->lastzone++);

  if (mydns_soa_load(sql, &soa, zone) == 0) {
#if DEBUG_ENABLED && DEBUG_NOTIFY
    DebugX("notify", 1, _("%s: DNS NOTIFY notify_all_soas loaded SOA for zone %s"),
	   desctask(t), zone);
#endif
    if (!soa->recursive) {
      notify_slaves(t, soa);
    }
    mydns_soa_free(soa);
  }
#if DEBUG_ENABLED && DEBUG_NOTIFY
  else {
    DebugX("notify", 1, _("%s: DNS NOTIFY notify_all_soas failed to load SOA for zone %s"),
	   desctask(t), zone);
  }
#endif

  initdata->zonecount -= 1;

#if DEBUG_ENABLED && DEBUG_NOTIFY
  DebugX("notify", 1,
	 _("%s: DNS NOTIFY notify_all_soas loaded a soa for notification zone %s number %d, zonesremaining %d"),
	 desctask(t), zone, initdata->lastzone, initdata->zonecount);
#endif

  return ((initdata->zonecount > 0)?TASK_CONTINUE:TASK_COMPLETED);
}

void
notify_start() {
  TASK *inittask = NULL;
  INITDATA *initdata = NULL;

  int zonecount = 0;

  if (!dns_notify_enabled) return;

  /*
   * Allocate a task to run through the SOA's and set up notify tasks for each.
   */
  if(!(zonecount = mydns_soa_count(sql))) {
    return;
  }

  initdata = (INITDATA*)ALLOCATE(sizeof(INITDATA), INITDATA);
  initdata->zonecount = zonecount;
  initdata->lastzone = -1;
  initdata->zones = NULL;

  inittask = Ticktask_init(LOW_PRIORITY_TASK, NEED_TASK_RUN, -1, 0, AF_UNSPEC, NULL);
  task_add_extension(inittask, initdata, notify_initfree, notify_all_soas, notify_all_soas);
  inittask->timeout = current_time + 10; /* Wait 10 seconds before firing first notify set */

}
/* vi:set ts=3: */
/* NEED_PO */
