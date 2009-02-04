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

#ifndef _MYDNS_TASKINT_H
#define _MYDNS_TASKINT_H

typedef struct _named_task *TASKP;

/* Task completion codes */
typedef enum _task_execstatus_t {
  TASK_ABANDONED	=-2,		/* Task needs to be abandoned - release fd */
  TASK_FAILED		=-1,		/* Task failed to execute properly kill */

  TASK_COMPLETED	= 0,		/* Task has run to completion dequeue */
  TASK_FINISHED		= 1,		/* Task finished normally free all resources */
  TASK_TIMED_OUT	= 2,		/* Task has timed out - dequeue */

  TASK_EXECUTED		= 3,		/* Task executed but did not complete retry later */
  TASK_DID_NOT_EXECUTE	= 4,		/* Task did not execute try again later */
  TASK_CONTINUE		= 5,		/* Task needs to run again */
} taskexec_t;

typedef enum _task_error_t					/* Common errors */
{
	ERR_NONE = 0,						/* No error */
	ERR_INTERNAL,						/* "Internal error" */
	ERR_ZONE_NOT_FOUND,					/* "Zone not found" */
	ERR_NO_MATCHING_RECORDS,				/* "No matching resource records" */
	ERR_NO_AXFR,						/* "AXFR disabled" */
	ERR_RR_NAME_TOO_LONG,					/* "Name too long in RR" */
	ERR_RR_LABEL_TOO_LONG,					/* "Label too long in RR" */
	ERR_Q_BUFFER_OVERFLOW,					/* "Input name buffer overflow" */
	ERR_Q_INVALID_COMPRESSION,				/* "Invalid compression method" */
	ERR_Q_NAME_TOO_LONG,					/* "Question name too long" */
	ERR_Q_LABEL_TOO_LONG,					/* "Question label too long" */
	ERR_NO_CLASS,						/* "Unknown class" */
	ERR_NAME_FORMAT,					/* "Invalid name format" */
	ERR_TIMEOUT,						/* "Communications timeout" */
	ERR_BROKEN_GLUE,					/* "Malformed glue" */
	ERR_INVALID_ADDRESS,					/* "Invalid address" */
	ERR_INVALID_TYPE,					/* "Invalid type" */
	ERR_INVALID_CLASS,					/* "Invalid class" */
	ERR_INVALID_TTL,					/* "Invalid TTL" (for update) */
	ERR_INVALID_DATA,					/* "Invalid data" (for update) */
	ERR_DB_ERROR,						/* "Database error" */
	ERR_NO_QUESTION,					/* "No question in query" */
	ERR_NO_AUTHORITY,					/* "No authority data" (for ixfr) */
	ERR_MULTI_QUESTIONS,					/* "Multiple questions in query" */
	ERR_MULTI_AUTHORITY,					/* "Multiple authority records in quer" */
	ERR_QUESTION_TRUNCATED,					/* "Question truncated" */
	ERR_UNSUPPORTED_OPCODE,					/* "Unsupported opcode" */
	ERR_UNSUPPORTED_TYPE,					/* "Unsupported type" */
	ERR_MALFORMED_REQUEST,					/* "Malformed request" */
	ERR_IXFR_NOT_ENABLED,					/* "IXFR not enabled" */
	ERR_TCP_NOT_ENABLED,					/* "TCP not enabled" */
	ERR_RESPONSE_BIT_SET,					/* "Response bit set on query" */
	ERR_FWD_RECURSIVE,					/* "Recursive query forwarding error" */
	ERR_NO_UPDATE,						/* "UPDATE denied" */
	ERR_PREREQUISITE_FAILED,				/* "UPDATE prerequisite failed" */

} task_error_t;

#endif
