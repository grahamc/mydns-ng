/**************************************************************************************************

	Copyright (C) 2009- Howard Wilkinson <howard@cohtech.com>

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

#include "array.h"
#include "memoryman.h"

#include "cache.h"
#include "data.h"
#include "listen.h"
#include "status.h"
#include "support.h"

#include "notify.h"

int	 	shutting_down = 0;		/* Shutdown in progress? */
char		hostname[256];			/* Hostname of local machine */

struct timeval	current_tick;			/* Current micro-second time */
time_t		current_time;			/* Current time */

struct timeval *gettick() {

  gettimeofday(&current_tick, NULL);
  current_time = current_tick.tv_sec;

  return (&current_tick);
}

/**************************************************************************************************
	NAMED_CLEANUP
**************************************************************************************************/
void named_cleanup(int signo) {

  shutting_down = signo;

}

void
named_shutdown(int signo) {
  int n = 0;

  switch (signo) {
  case 0:
    Notice(_("Normal shutdown")); break;
  case SIGINT:
    Notice(_("interrupted")); break;
  case SIGQUIT:
    Notice(_("quit")); break;
  case SIGTERM:
    Notice(_("terminated")); break;
  case SIGABRT:
    Notice(_("aborted")); break;
  default:
    Notice(_("exiting due to signal %d"), signo); break;
  }

  server_status();

  task_free_all();

  cache_empty(ZoneCache);
#if USE_NEGATIVE_CACHE
  cache_empty(NegativeCache);
#endif
  cache_empty(ReplyCache);

  listen_close_all();

}

/**************************************************************************************************
	SERVER_STATUS
**************************************************************************************************/
void server_status(void) {
  char			buf[1024], *b = buf;
  time_t		uptime = time(NULL) - Status.start_time;
  unsigned long 	requests = Status.udp_requests + Status.tcp_requests;

  b += snprintf(b, sizeof(buf)-(b-buf), "%s ", hostname);
  b += snprintf(b, sizeof(buf)-(b-buf), "%s %s (%lus) ", _("up"), strsecs(uptime),
		(unsigned long)uptime);
  b += snprintf(b, sizeof(buf)-(b-buf), "%lu %s ", requests, _("questions"));
  b += snprintf(b, sizeof(buf)-(b-buf), "(%.0f/s) ", requests ? AVG(requests, uptime) : 0.0);
  b += snprintf(b, sizeof(buf)-(b-buf), "NOERROR=%u ", Status.results[DNS_RCODE_NOERROR]);
  b += snprintf(b, sizeof(buf)-(b-buf), "SERVFAIL=%u ", Status.results[DNS_RCODE_SERVFAIL]);
  b += snprintf(b, sizeof(buf)-(b-buf), "NXDOMAIN=%u ", Status.results[DNS_RCODE_NXDOMAIN]);
  b += snprintf(b, sizeof(buf)-(b-buf), "NOTIMP=%u ", Status.results[DNS_RCODE_NOTIMP]);
  b += snprintf(b, sizeof(buf)-(b-buf), "REFUSED=%u ", Status.results[DNS_RCODE_REFUSED]);

  /* If the server is getting TCP queries, report on the percentage of TCP queries */
  if (Status.tcp_requests)
    b += snprintf(b, sizeof(buf)-(b-buf), "(%d%% TCP, %lu queries)",
		  (int)PCT(requests, Status.tcp_requests),
		  (unsigned long)Status.tcp_requests);

  Notice("%s", buf);
}
/*--- server_status() ---------------------------------------------------------------------------*/

/**************************************************************************************************
	MYDNS_NAME_2_SHORTNAME
	Removes the origin from a name if it is present.
**************************************************************************************************/
char *mydns_name_2_shortname(char *name, char *origin, int empty_name_is_ok, int notrim) {
  size_t nlen = 0, olen = 0;
  int name_is_dotted, origin_is_dotted;

  if (name) nlen = strlen(name); else return name;
  if (origin) olen = strlen(origin); else return name;

  if (notrim)
    return (name);

  name_is_dotted = (LASTCHAR(name) == '.');
  origin_is_dotted = (LASTCHAR(origin) == '.');

  if (name_is_dotted && !origin_is_dotted) nlen -= 1;
  if (origin_is_dotted && !name_is_dotted) olen -= 1;

  if (nlen < olen)
    return (name);

  if (!strncasecmp(origin, name, nlen)) {
    if (empty_name_is_ok)
      return ("");
    else
      return (name);
  }
  if (!strncasecmp(name + nlen - olen, origin, olen)
      && name[nlen - olen - 1] == '.')
    name[nlen - olen - 1] = '\0';
  return (name);
}
/*--- mydns_name_2_shortname() -----------------------------------------------------------------*/

int
name_servers2ip(TASK *t, MYDNS_SOA *soa, ARRAY *name_servers,
		ARRAY *ips4,
		ARRAY *ips6
		) {
  ARRAY *ipaddresses = array_init(16);
  int i = 0;

#if DEBUG_ENABLED
  DebugX("support", 1, _("%s: name_servers2ip() called name_servers = %p"),
	 desctask(t), name_servers);
#endif

  if(!name_servers) return 0;

#if DEBUG_ENABLED
  DebugX("support", 1, _("%s: name_servers2ip() called name_servers count = %d"),
	 desctask(t), array_numobjects(name_servers));
#endif

  for (i = 0; i < array_numobjects(name_servers); i++) {
    char	*name_server = array_fetch(name_servers, i);
    char	*label = NULL;
    int		resolved = 0;

    MYDNS_SOA *nsoa = NULL;
    MYDNS_RR *rr = NULL;

#if DEBUG_ENABLED
    DebugX("support", 1, _("%s: name_servers2ip() processing name server %s"),
	   desctask(t), name_server);
#endif

    nsoa = find_soa(t, name_server, &label);

    if (nsoa) {
      MYDNS_RR *r = NULL;

#if DEBUG_ENABLED
      DebugX("support", 1, _("%s: name_servers2ip() processing rr %s for name server %s"),
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
	DebugX("support", 1, _("%s: name_servers2ip() processing name server %s got ip address %s(%d)"),
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
	DebugX("support", 1,
	       _("%s: name_servers2ip() processing name server %s append IP address to saved vector"),
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
	DebugX("support", 1, _("%s: name_servers2ip() processing name server %s got ip address %s(%d)"),
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
	DebugX("support", 1,
	       _("%s: name_servers2ip() processing name server %s append IP address to saved vector"),
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
    DebugX("support", 1, _("%s: name_servers2ip() - done try local DNS - %d"), desctask(t), resolved);
#endif

    /* This needs enhancing so that a recursive lookup is run before trying the local DNS */

    /* Look up via other DNS/hosts instead */
#if HAVE_GETADDRINFO
    {
      struct addrinfo hosthints;
      struct addrinfo *hostdata = NULL;
      struct addrinfo *hostentry;

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

      int rv = getaddrinfo(name_server, NULL, NULL, &hostdata);

      if (!rv) {
	int j, k;

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
  DebugX("support", 1,
	 _("%s: name_servers2ip() - releasing data and then returning ipaddresses=%p, name_servers=%p"),
	 desctask(t), ipaddresses, name_servers);
#endif

  array_free(ipaddresses, 1);
  array_free(name_servers, 1);

#if DEBUG_ENABLED
  DebugX("support", 1, _("%s: name_servers2ip() - returning"), desctask(t));
#endif

  return array_numobjects(ips4)
#if HAVE_IPV6
    + array_numobjects(ips6)
#endif
    ;
}

/* vi:set ts=3: */
/* NEED_PO */
