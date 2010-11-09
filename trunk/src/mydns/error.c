/**************************************************************************************************
	$Id: error.c,v 1.30 2006/01/18 20:46:47 bboy Exp $

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
#define	DEBUG_ERROR	1


/* The maximum number of resource record ID errors that we'll remember (and avoid repeating) */
#define	MAX_RR_ERR_MEMORY	1024

uint32_t	rr_err_memory[MAX_RR_ERR_MEMORY];


/**************************************************************************************************
	ERR_REASON_STR
	Returns a string describing 'reason'.
**************************************************************************************************/
char *
err_reason_str(TASK *t, task_error_t reason) {
  static char *buf = NULL;

  switch (reason) {
  case ERR_NONE:			return ((char *)"-");
  case ERR_INTERNAL: 			return ((char *)_("Internal_error"));
  case ERR_ZONE_NOT_FOUND: 		return ((char *)_("Zone_not_found"));
  case ERR_NO_MATCHING_RECORDS: 	return ((char *)_("No_matching_resource_records"));
  case ERR_NO_AXFR: 			return ((char *)_("AXFR_disabled"));
  case ERR_RR_NAME_TOO_LONG: 		return ((char *)_("Name_too_long_in_RR"));
  case ERR_RR_LABEL_TOO_LONG: 		return ((char *)_("Label_too_long_in_RR"));
  case ERR_Q_BUFFER_OVERFLOW: 		return ((char *)_("Input_name_buffer_overflow"));
  case ERR_Q_INVALID_COMPRESSION:	return ((char *)_("Invalid_compression_method"));
  case ERR_Q_NAME_TOO_LONG: 		return ((char *)_("Question_name_too_long"));
  case ERR_Q_LABEL_TOO_LONG: 		return ((char *)_("Question_label_too_long"));
  case ERR_NO_CLASS: 			return ((char *)_("Unsupported_class"));
  case ERR_NAME_FORMAT: 		return ((char *)_("Invalid_name_format"));
  case ERR_TIMEOUT: 			return ((char *)_("Communications_timeout"));
  case ERR_BROKEN_GLUE: 		return ((char *)_("Malformed_glue"));
  case ERR_INVALID_ADDRESS: 		return ((char *)_("Invalid_address"));
  case ERR_INVALID_TYPE: 		return ((char *)_("Invalid_type"));
  case ERR_INVALID_CLASS: 		return ((char *)_("Invalid_class"));
  case ERR_INVALID_TTL: 		return ((char *)_("Invalid_TTL"));
  case ERR_INVALID_DATA: 		return ((char *)_("Invalid_data"));
  case ERR_DB_ERROR: 			return ((char *)_("Database_error"));
  case ERR_NO_QUESTION: 		return ((char *)_("No_question_in_query"));
  case ERR_NO_AUTHORITY:		return ((char *)_("No_authority_in_query"));
  case ERR_MULTI_QUESTIONS: 		return ((char *)_("Multiple_questions_in_query"));
  case ERR_MULTI_AUTHORITY:		return ((char *)_("Multiple_authority_records_in_ixfr_query"));
  case ERR_QUESTION_TRUNCATED: 		return ((char *)_("Question_truncated"));
  case ERR_UNSUPPORTED_OPCODE:
    if (buf) RELEASE(buf);
    ASPRINTF(&buf,
	     "%s_%s",
	     _("Unsupported_opcode"),
	     mydns_opcode_str(t->hdr.opcode));
    return buf;
  case ERR_UNSUPPORTED_TYPE: 		return ((char *)_("Unsupported_type"));
  case ERR_MALFORMED_REQUEST: 		return ((char *)_("Malformed_request"));
  case ERR_IXFR_NOT_ENABLED: 		return ((char *)_("IXFR_not_enabled"));
  case ERR_TCP_NOT_ENABLED: 		return ((char *)_("TCP_not_enabled"));
  case ERR_RESPONSE_BIT_SET: 		return ((char *)_("Response_bit_set_on_query"));
  case ERR_FWD_RECURSIVE: 		return ((char *)_("Recursive_query_forwarding_error"));
  case ERR_NO_UPDATE: 			return ((char *)_("UPDATE_denied"));
  case ERR_PREREQUISITE_FAILED: 	return ((char *)_("UPDATE_prerequisite_failed"));
  }
  return ((char *)_("Unknown"));
}
/*--- err_reason_str() --------------------------------------------------------------------------*/


/**************************************************************************************************
	_FORMERR_INTERNAL
	Called during parsing of request if an error occurs.  Returns NULL.
**************************************************************************************************/
taskexec_t
_formerr_internal(
	TASK *t,			/* The failed task */
	dns_rcode_t rcode,		/* The return code to use, such as DNS_RCODE_SERVFAIL, etc.*/
	task_error_t reason,		/* Further explanation of the error */
	char *xtra,			/* Extra information (displayed if in debug mode) */
	const char *filename,
	unsigned int lineno
) {
  char	*dest = NULL;

#if DEBUG_ENABLED && DEBUG_ERROR
  DebugX("error", 1, _("%s: formerr(): %s %s from %s:%u: %s"),
	 desctask(t), mydns_rcode_str(rcode), err_reason_str(t, reason), filename, lineno,
	 xtra ?: _("no additional information"));
#endif

  t->hdr.rcode = rcode;
  t->status = NEED_WRITE;
  t->reason = reason;

  /* Build simple reply to avoid problems with malformed data */
  t->replylen = DNS_HEADERSIZE;
  dest = t->reply = ALLOCATE(t->replylen, char[]);
  t->hdr.qr = 1;
  DNS_PUT16(dest, t->id);						/* Query ID */
  DNS_PUT(dest, &t->hdr, SIZE16);					/* Header */
  DNS_PUT16(dest, 0);							/* QUESTION count */
  DNS_PUT16(dest, 0);							/* ANSWER count */
  DNS_PUT16(dest, 0);							/* AUTHORITY count */
  DNS_PUT16(dest, 0);							/* ADDITIONAL count */

  return (TASK_FAILED);
}
/*--- _formerr_internal() -----------------------------------------------------------------------*/


/**************************************************************************************************
	_DNSERROR_INTERNAL
	Generate error response for query and set status to NEED_SEND.  Always returns -1, as a
	convenience.
**************************************************************************************************/
taskexec_t
_dnserror_internal(
	TASK *t,					/* The failed task */
	dns_rcode_t rcode,	/* The return code to use, such as DNS_RCODE_SERVFAIL, etc. */
	task_error_t reason,	/* Further explanation of the error */
	const char *filename,
	unsigned int lineno
) {
  if (t->hdr.rcode == DNS_RCODE_NOERROR) {
#if DEBUG_ENABLED && DEBUG_ERROR
    DebugX("error", 1, _("%s: dnserror(): %s %s from %s:%u"),
	   desctask(t), mydns_rcode_str(rcode), err_reason_str(t, reason), filename, lineno);
#endif
    t->hdr.rcode = rcode;
    t->status = NEED_WRITE;
    t->reason = reason;
  }

  return (TASK_FAILED);
}
/*--- _dnserror_internal() ----------------------------------------------------------------------*/


/**************************************************************************************************
	RR_ERROR_REPEAT
	If the program has already reported on the specified RR as having an error, returns 1, else
	0.  Maintains an internal list of the past 1024 RR id's that have been reported.
**************************************************************************************************/
int
rr_error_repeat(uint32_t id) {
  register int n = 0;

  for (n = 0; n < MAX_RR_ERR_MEMORY; n++)
    if (rr_err_memory[n] == id)
      return 1;

  /* Move rest of list back and add 'id' to beginning of list */
  for (n = MAX_RR_ERR_MEMORY; n > 0; n--)
    rr_err_memory[n] = rr_err_memory[n-1];
  rr_err_memory[0] = id;
  return 0;
}
/*--- rr_error_repeat() -------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_ERROR
	Report on an error in a resource record.
**************************************************************************************************/
int
rr_error(uint32_t id, const char *fmt, ...) {
  if (show_data_errors && !rr_error_repeat(id)) {
    char *msg = NULL;
    va_list ap;

    /* Construct output string */
    va_start(ap, fmt);
    VASPRINTF(&msg, fmt, ap);
    va_end(ap);

    Warnx("%s", msg);
    RELEASE(msg);
  }
  return -1;
}
/*--- rr_error() --------------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */
