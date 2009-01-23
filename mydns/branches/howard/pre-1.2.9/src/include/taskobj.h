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

#ifndef _MYDNS_TASKOBJ_H
#define _MYDNS_TASKOBJ_H

#include "taskint.h"
#include "mydns.h"
#include "header.h"
#include "queue.h"

/* If defined, DYNAMIC_NAMES causes dynamic allocation of the encoded names list.  It's slow. */
#define	DYNAMIC_NAMES	0

#define	MAX_CNAME_LEVEL	6

/* Task type select */
typedef enum _taskpriority_t {
  HIGH_PRIORITY_TASK = 0,
  NORMAL_PRIORITY_TASK = 1,
  LOW_PRIORITY_TASK = 2,
} taskpriority_t;

typedef enum _tasktype_t {
  NORMAL_TASK = 0,			/* Old style task */
  IO_TASK = 1,				/* New task dependent on IO event to run */
  PERIODIC_TASK = 2,			/* Task that runs on a tick */
} tasktype_t;

#define Needs2Read		0x0001
#define Needs2Write		0x0002
#define Needs2Connect		0x0004
#define Needs2Exec		0x0008

#define Needs2Recurse		0x0010

/*
 * Use the following scheme for task status
 *
 * Split the enum into 3 fields -
 *	 upper 16 bits are operation identifiers
 *	 upper byte of lower 16 bits is the class of operation being processed
 *	 lower byte of lower 16 bits contains bit mask of type of operation as above
 *
 *	A Query Task - i.e. incoming request, outgoing response
 *      A Request Task - i.e. outgoing request, incoming response e.g. Notify
 *	A Periodic Task
 *	
 */
#define QueryTask		0x0100
#define ReqTask			0x0200
#define TickTask		0x0400
#define RunTask			0x0800

#define TASKCLASS(n)		((n)&(QueryTask|ReqTask|TickTask|RunTask))
#define TASKIOTYPE(n)		((n)&(Needs2Read|Needs2Write|Needs2Connect|Needs2Exec))
#define TaskIsRecursive(n)	((n)&Needs2Recurse)
#define TASKSTAT(n)		((n)<<16)
#define TASKSTATVAL(n)		(((n)>>16)&0xFFFF)
#define TASKTIMESOUT(n)		((n)&(QueryTask|ReqTask|TickTask))

/* Task status flags */
typedef enum _taskstat_t {
  /* We need to read the question */
  NEED_READ = TASKSTAT(0)|QueryTask|Needs2Read,
  /* We need to find the answer */
  NEED_ANSWER = TASKSTAT(1)|QueryTask|Needs2Exec,
  /* We need to write the answer */
  NEED_WRITE = TASKSTAT(2)|QueryTask|Needs2Write,
  /* We need to process an IXFR request */
  NEED_IXFR = TASKSTAT(3)|QueryTask|Needs2Exec,

  /* Need to open connection to recursive server */
  NEED_RECURSIVE_FWD_CONNECT = TASKSTAT(0)|QueryTask|Needs2Connect|Needs2Recurse,
  /* TCP connection has started but not finished */
  NEED_RECURSIVE_FWD_CONNECTING = TASKSTAT(1)|QueryTask|Needs2Write|Needs2Recurse,
  /* Need to write the question to recursive forwarder */
  NEED_RECURSIVE_FWD_WRITE = TASKSTAT(2)|QueryTask|Needs2Write|Needs2Recurse,
  /* Have sent message will need to retry on timeout */
  NEED_RECURSIVE_FWD_RETRY = TASKSTAT(3)|TickTask|Needs2Exec|Needs2Recurse,
  /* Need to read the answer from recursive forwarder */
  NEED_RECURSIVE_FWD_READ = TASKSTAT(4)|QueryTask|Needs2Read|Needs2Recurse,

  /* Need to read a response to a notify */
  NEED_NOTIFY_READ = TASKSTAT(0)|ReqTask|Needs2Read,
  /* Need to write a notify to a server */
  NEED_NOTIFY_WRITE = TASKSTAT(1)|ReqTask|Needs2Write,
  /* Need to retry if slaves do not reply */
  NEED_NOTIFY_RETRY = TASKSTAT(2)|TickTask|Needs2Exec,

  /* Need to run these tasks */
  NEED_TASK_RUN = TASKSTAT(0)|RunTask|Needs2Exec,
  NEED_AXFR = TASKSTAT(1)|RunTask|Needs2Exec,
  NEED_TASK_READ = TASKSTAT(2)|RunTask|Needs2Read,

  /* Interprocess commands */
  NEED_COMMAND_READ = TASKSTAT(3)|QueryTask|Needs2Read,
  NEED_COMMAND_WRITE = TASKSTAT(4)|QueryTask|Needs2Write,
} taskstat_t;


/* RR: A single resource record (of any supported type) */
typedef struct _named_rr {
  dns_rrtype_t		rrtype;			/* Record type (what table this data came from) */
  uint32_t		id;			/* ID associated with RR */
  unsigned char		name[DNS_MAXNAMELEN];	/* Name to send with reply */
  off_t			offset;			/* The offset within the reply data (t->rdata) */
  size_t		length;			/* The length of data within the reply */
  uint8_t		sort_level;		/* Primary sort order */
  uint32_t		sort1, sort2;		/* Sort order within level */
  unsigned int		lb_low, lb_high;	/* High/low values for load balancing (ugh) */
  void			*rr;			/* The RR data */

  struct _named_rr	*next;			/* Pointer to the next item */
} RR;

/* RRLIST: A list of resource records */
typedef struct _named_rrlist {
  size_t	        size;			/* Count of records */

  int			a_records;		/* Number of A or AAAA records (for sorting) */
  int			mx_records;		/* Number of MX records (for sorting) */
  int			srv_records;		/* Number of SRV records (for sorting) */

  RR	       		*head;			/* Head of list */
  RR	       		*tail;			/* Tail of list */
} RRLIST;

typedef void (*FreeExtension)(/* TASK*, void* */);
typedef taskexec_t (*RunExtension)(/* TASK*, void* */);
typedef taskexec_t (*TimeExtension)(/* TASK*, void* */);

/* TASK: DNS query task */
typedef struct _named_task {
  QueueEntry		queue_header;
  tasktype_t		type;
  taskpriority_t	priority;

  uint16_t		internal_id;		/* Internal task ID */
  taskstat_t		status;			/* Current status of query */
  time_t		timeout;		/* Time task expires (timeout) */

  void			*extension;		/* Data for new tasking model */
  FreeExtension		freeextension;		/* Free extension data */
  RunExtension		runextension;		/* Run extension */
  TimeExtension		timeextension;		/* Extension timed out */

  /* IO Tasks */
  int			fd;			/* Socket FD */
  int			protocol;		/* Type of socket (SOCK_DGRAM/SOCK_STREAM) */
  int			family;			/* Socket family (AF_INET/AF_INET6) */

  struct sockaddr_in	addr4;			/* IPv4 address of client */
#if HAVE_IPV6
  struct sockaddr_in6	addr6;			/* IPv6 address of client */
#endif

  /* I/O information for TCP queries */
  size_t		len;			/* Query length */
  char	       		*query;			/* Query data */
  size_t		offset;			/* Current offset */
  int			len_written;		/* Have we written length octets? */

  /* Query information */
  uint32_t		minimum_ttl;		/* Minimum TTL for current zone */
  uint16_t		id;			/* Query ID */
  DNS_HEADER		hdr;			/* Header */
  dns_class_t		qclass;			/* Query class */
  dns_qtype_t		qtype;			/* Query type */
  char			qname[DNS_MAXNAMELEN];	/* Query name object */
  task_error_t		reason;			/* Further explanation of the error */

  uint32_t		Cnames[MAX_CNAME_LEVEL];/* Array of CNAMEs found */

  unsigned char  	*qd;			/* Question section data */
  size_t		qdlen;			/* Size of question section */
  uint16_t		qdcount;		/* "qdcount", from header */
  uint16_t		ancount;		/* "ancount", from header */
  uint16_t		nscount;		/* "nscount", from header */
  uint16_t		arcount;		/* "arcount", from header */

  int			no_markers;		/* Do not use markers? */

#if DYNAMIC_NAMES
  char	      		**Names;		/* Names stored in reply */
  unsigned int   	*Offsets;		/* Offsets for names */
#else
#define	MAX_STORED_NAMES	128
  char			Names[MAX_STORED_NAMES][DNS_MAXNAMELEN + 1];	/* Names stored in reply */
  unsigned int		Offsets[MAX_STORED_NAMES];			/* Offsets for names */
#endif

  unsigned int		numNames;		/* Number of names in the list */

  uint32_t		zone;			/* Zone ID */

  uint8_t		sort_level;		/* Current sort level */

  RRLIST		an, ns, ar;		/* RR's for ANSWER, AUTHORITY, ADDITIONAL */

  char	       		*rdata;			/* Header portion of reply */
  size_t		rdlen;			/* Length of `rdata' */

  char	       		*reply;			/* Total constructed reply data */
  size_t		replylen;		/* Length of `reply' */

  int			reply_from_cache;	/* Did reply come from reply cache? */

  int			reply_cache_ok;		/* Can we cache this reply? */
  int			name_ok;		/* Does _some_ record match the name? */

  int			forwarded;		/* Forwarded to a recursive server? */

  int			update_done;		/* Did we do any dynamic updates? */
  int			info_already_out;	/* Has the info already been output? */
} TASK;

extern QUEUE		*TaskArray[PERIODIC_TASK+1][LOW_PRIORITY_TASK+1];

extern uint32_t 	answer_then_quit;		/* Answer this many queries then quit */

extern TASK 		*task_find_by_id(TASK *, QUEUE *, unsigned long);

extern TASK		*_task_init(tasktype_t, taskpriority_t, taskstat_t, int, int, int, void *, const char *, int);
#define			task_init(P,S,fd,p,f,a)	_task_init(NORMAL_TASK, (P), (S), (fd), (p), (f), (a), __FILE__, __LINE__)
#define			IOtask_init(P,S,fd,p,f,a)	_task_init(IO_TASK, (P), (S), (fd), (p), (f), (a), __FILE__, __LINE__)
#define			Ticktask_init(P,S,fd,p,f,a)	_task_init(PERIODIC_TASK, (P), (S), (fd), (p), (f), (a), __FILE__, __LINE__)

extern void		_task_change_type(TASK *, tasktype_t, taskpriority_t);
#define			task_change_type(t,T) _task_change_type((t), (T), (t)->priority)
#define			task_change_priority(t,P) _task_change_type((t),(t)->type,(P))
#define			task_change_type_and_priority(t,T,P) _task_change_type((t),(T),(P))

extern void		_task_free(TASK *, const char *, int);
#define			task_free(T)	if ((T)) _task_free((T), __FILE__, __LINE__), (T) = NULL

extern void		task_add_extension(TASK*, void*, FreeExtension, RunExtension, TimeExtension);
extern void		task_remove_extension(TASK *);

extern void		task_output_info(TASK *, char *);
extern char		*task_exec_name(taskexec_t);
extern char		*task_type_name(int);
extern char		*task_priority_name(int);
extern char		*task_string_name(TASK *);
extern const char	*clientaddr(TASK *);
extern char		*desctask(TASK *);

extern int		_task_enqueue(QUEUE **, TASK *, const char *, unsigned int);
extern void		_task_dequeue(QUEUE **, TASK *, const char *, unsigned int);
extern void		_task_requeue(QUEUE **, TASK *, const char *, unsigned int);

#define			enqueue(Q,T)	_task_enqueue((Q), (T), __FILE__, __LINE__)
#define			dequeue(T)	_task_dequeue((T)->queue_header.Q, (T), __FILE__, __LINE__)
#define			requeue(Q, T)	_task_requeue((Q), (T), __FILE__, __LINE__)

extern void		task_queue_stats();

#define task_next(T)	((TASK*)((T)->queue_header.next))
#define task_prev(T)	((TASK*)((T)->queue_header->prev))
#define task_queue(T)	((T)->queue_header.Q)

extern void		task_free_all();

#endif /* !_MYDNS_TASK_H */
/* vi:set ts=3: */
