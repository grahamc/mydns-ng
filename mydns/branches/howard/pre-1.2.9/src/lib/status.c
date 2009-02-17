/**************************************************************************************************
	$Id: status.c,v 1.13 2006/01/18 20:46:47 bboy Exp $

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

#include "memoryman.h"

#include "error.h"
#include "rr.h"
#include "rrtype.h"
#include "status.h"

#if DEBUG_ENABLED
int		debug_status = 0;
#endif

SERVERSTATUS	Status;				/* Server status information */

/**************************************************************************************************
	STATUS_FAKE_RR
	Generates a fake TXT rr and adds it to the requested data section.
**************************************************************************************************/
static void
status_fake_rr(TASK *t, datasection_t ds, const char *name, const char *fmt, ...) {
  va_list	ap;
  char		*buf = NULL;
  MYDNS_RR 	*rr = NULL;					/* Temporary resource record */

#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_fake_rr called"), desctask(t));
#endif
  va_start(ap, fmt);
  VASPRINTF(&buf, fmt, ap);
  va_end(ap);

  rr = mydns_rr_build(0, 0, mydns_rr_get_type_by_id(DNS_QTYPE_TXT), DNS_CLASS_CHAOS,
		      0, 0, NULL, NULL, 0, (char*)name, buf, strlen(buf), NULL);
  /* Add to list */
  rrlist_add(t, ds, DNS_RRTYPE_RR, (void *)rr, (char*)name);
  mydns_rr_free(rr);
  RELEASE(buf);
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_fake_rr returns"), desctask(t));
#endif
}
/*--- status_fake_rr() --------------------------------------------------------------------------*/


/**************************************************************************************************
	STATUS_VERSION_BIND
	Respond to 'version.bind.' query.
**************************************************************************************************/
static int
status_version_bind(TASK *t) {
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_version_bind called"), desctask(t));
#endif
  /* Generate fake TXT rr with version number and add to reply list */
  status_fake_rr(t, ANSWER, t->qname, "%s", VERSION);

#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_version_bind returns COMPLETED"), desctask(t));
#endif
  return TASK_COMPLETED;
}
/*--- status_version_bind() ---------------------------------------------------------------------*/


/**************************************************************************************************
	STATUS_VERSION_MYDNS
	Respond to 'version.mydns.' query.
**************************************************************************************************/
static int
status_version_mydns(TASK *t) {
  time_t uptime = time(NULL) - Status.start_time;
  unsigned long requests = Status.udp_requests + Status.tcp_requests;
  int n = 0;

#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_version_mydns called"), desctask(t));
#endif
  /* Generate fake TXT rr with version number and add to reply list */
  status_fake_rr(t, ANSWER, t->qname, "%s", VERSION);

  /* Now add some extra stuff in ADDITIONAL section */

  /* Package release date */
  status_fake_rr(t, ADDITIONAL, "release-date.mydns.", "%s", PACKAGE_RELEASE_DATE);

  /* Current uptime */
  status_fake_rr(t, ADDITIONAL, "uptime.mydns.", "%s", strsecs(uptime));

  /* Number of requests */
  status_fake_rr(t, ADDITIONAL, "requests.mydns.", "%lu", requests);

  /* Request rate */
  status_fake_rr(t, ADDITIONAL, "request.rate.mydns.", "%.0f/s", requests ? AVG(requests, uptime) : 0.0);

  /* Report TCP requests if server allows TCP */
  if (Status.tcp_requests)
    status_fake_rr(t, ADDITIONAL, "tcp.requests.mydns.", "%lu", Status.tcp_requests);

  /* Result status */
  for (n = 0; n < MAX_RESULTS; n++) {
    if (Status.results[n]) {
      char *namebuf = NULL;

      ASPRINTF(&namebuf, "results.%s.mydns.", mydns_rcode_str(n));
      status_fake_rr(t, ADDITIONAL, namebuf, "%u", Status.results[n]);
      RELEASE(namebuf);
    }
  }

#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_version_mydns returns COMPLETED"), desctask(t));
#endif
  return TASK_COMPLETED;
}
/*--- status_version_mydns() --------------------------------------------------------------------*/


/**************************************************************************************************
	STATUS_GET_RR
**************************************************************************************************/
taskexec_t status_get_rr(TASK *t) {
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_get_rr called"), desctask(t));
#endif
  if (t->qtype != DNS_QTYPE_TXT) {
#if DEBUG_ENABLED
    Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_get_rr returns RCODE_NOTIMP"), desctask(t));
#endif
    return formerr(t, DNS_RCODE_NOTIMP, ERR_NO_CLASS, NULL);
  }

  /* Emulate BIND's 'version.bind.' ("dig txt chaos version.bind") */
  if (!strcasecmp(t->qname, "version.bind.")) {
#if DEBUG_ENABLED
    Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_get_rr returns bind status"), desctask(t));
#endif
    return status_version_bind(t);
  }

  /* Extended MyDNS 'version.mydns.' ("dig txt chaos version.mydns") */
  else if (!strcasecmp(t->qname, "version.mydns.")) {
#if DEBUG_ENABLED
    Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_get_rr returns mydns status"), desctask(t));
#endif
    return status_version_mydns(t);
  }

#if DEBUG_ENABLED
    Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_get_rr returns RCODE_NOTIMP"), desctask(t));
#endif
  return formerr(t, DNS_RCODE_NOTIMP, ERR_NO_CLASS, NULL);
}
/*--- status_get_rr() ---------------------------------------------------------------------------*/

void status_task_timedout(TASK *t) {
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_task_timedout called"), desctask(t));
#endif
  Status.timedout++;
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_task_timedout returns"), desctask(t));
#endif
}

void status_start_server() {
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("status_start_server called"));
#endif
  time(&Status.start_time);
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("status_start_server returns"));
#endif
}

void status_tcp_request(TASK *t) {
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_tcp_request called"), desctask(t));
#endif
  Status.tcp_requests++;
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_tcp_request returns"), desctask(t));
#endif
}

void status_udp_request(TASK *t) {
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_udp_request called"), desctask(t));
#endif
  Status.udp_requests++;
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_udp_request returns"), desctask(t));
#endif
}

void status_result(TASK *t, int rcode) {
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_result called"), desctask(t));
#endif
  if (rcode >= 0 && rcode < MAX_RESULTS) {
    Status.results[rcode]++;
  }
#if DEBUG_ENABLED
  Debug(status, DEBUGLEVEL_FUNCS, _("%s: status_result returns"), desctask(t));
#endif
}
/* vi:set ts=3: */
