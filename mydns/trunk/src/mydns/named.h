/**************************************************************************************************
	$Id: named.h,v 1.65 2005/04/20 16:49:12 bboy Exp $

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

#ifndef _MYDNS_NAMED_H
#define _MYDNS_NAMED_H

typedef struct _named_queue *QUEUEP;

#include "mydnsutil.h"
#include "header.h"
#include "mydns.h"
#include "task.h"
#include "cache.h"

#if HAVE_SYS_RESOURCE_H
#	include <sys/resource.h>
#endif

#if HAVE_SYS_WAIT_H
#	include <sys/wait.h>
#endif

#if HAVE_NETDB_H
#	include <netdb.h>
#endif


/* The alarm function runs every ALARM_INTERVAL seconds */
#define		ALARM_INTERVAL		15
#define		DNS_MAXPACKETLEN	DNS_MAXPACKETLEN_UDP
#if ALIAS_ENABLED
#	define	MAX_ALIAS_LEVEL	6
#endif

/* Maximum CNAME recursion depth */
#define	DNS_MAX_CNAME_RECURSION	25

/* Size of reply header data; that's id + DNS_HEADER + qdcount + ancount + nscount + arcount */
#define	DNS_HEADERSIZE		(SIZE16 * 6)


#define SQLESC(s,d) { \
		char *rv = alloca(strlen((s))*2+1); \
		if (!rv) Err("alloca"); \
		sql_escstr(sql, rv, s, strlen((s))); \
		d = rv; \
	}

/* This is the header offset at the start of most reply functions.
	The extra SIZE16 at the end is the RDLENGTH field in the RR's header. */
#define CUROFFSET(t) (DNS_HEADERSIZE + (t)->qdlen + (t)->rdlen + SIZE16)


#if DEBUG_ENABLED
extern const char *datasection_str[];			/* Strings describing data section types */
#endif

/* Structure for ARRAY objects */
typedef struct _named_array {
  size_t	size;
  int		maxidx;
  void		**objects;
} ARRAY;

/* Queue structure for TASK records (really not a queue, but a list) */
typedef struct _named_queue
{
	char *queuename;
	size_t	size;					/* Number of elements in queue */
  	size_t	max_size;
	TASK	*head;					/* Pointer to first element in list */
	TASK	*tail;					/* Pointer to last element in list */
} QUEUE;


#define MAX_RESULTS	20
typedef struct _serverstatus									/* Server status information */
{
	time_t	start_time;	 										/* Time server started */
	uint32_t	udp_requests, tcp_requests;					/* Total # of requests handled */
	uint32_t	timedout;	 										/* Number of requests that timed out */
	uint32_t	results[MAX_RESULTS];							/* Result codes */
} SERVERSTATUS;

extern SERVERSTATUS Status;

typedef struct _named_server {
  pid_t		pid;
  int		serverfd;
  TASK		*listener;
  int		signalled;
} SERVER;

extern ARRAY	*Servers;


/* Global variables */
extern QUEUE	*TaskArray[PERIODIC_TASK+1][LOW_PRIORITY_TASK+1];

extern int	max_used_fd;

extern CACHE	*Cache;				/* Zone cache */
extern time_t	current_time;			/* Current time */

#if ALIAS_ENABLED
/* alias.c */
extern int	 alias_recurse(TASK *t, datasection_t section, char *fqdn, MYDNS_SOA *soa, char *label, MYDNS_RR *alias);
#endif

/* array.c */
extern ARRAY		*array_init(size_t);
extern void		array_free(ARRAY *, int);
extern void		array_append(ARRAY *, void *);
extern void		*array_remove(ARRAY *);
#define array_fetch(A,I)	(((A)->maxidx>=(I))?((A)->objects[(I)]):NULL)
#define array_store(A,I,O)	((A)->objects[(I)] = (O))
#define array_max(A)		((A)->maxidx)
#define array_numobjects(A)	(array_max((A))+1)
/* axfr.c */
extern void		axfr(TASK *);
extern void		axfr_fork(TASK *);

/* data.c */
extern MYDNS_SOA	*find_soa(TASK *, char *, char *);
extern MYDNS_SOA	*find_soa2(TASK *, char *, char **);
extern MYDNS_RR		*find_rr(TASK *, MYDNS_SOA *, dns_qtype_t, const char *);


/* encode.c */
extern int		name_remember(TASK *, const char *, unsigned int);
extern void		name_forget(TASK *);
extern unsigned int	name_find(TASK *, const char *);
extern int		name_encode(TASK *, char *, const char *, unsigned int, int);
extern int		name_encode2(TASK *, char **, const char *, unsigned int, int);


/* error.c */
extern taskexec_t	_formerr_internal(TASK *, dns_rcode_t, task_error_t, char *, const char *, unsigned int);
extern taskexec_t	_dnserror_internal(TASK *, dns_rcode_t, task_error_t, const char *, unsigned int);
extern char		*err_reason_str(TASK *, task_error_t);
extern int		rr_error_repeat(uint32_t);
extern int		rr_error(uint32_t, const char *, ...) __printflike(2,3);

#define formerr(task,rcode,reason,xtra)	_formerr_internal((task),(rcode),(reason),(xtra),__FILE__,__LINE__)
#define dnserror(task,rcode,reason)			_dnserror_internal((task),(rcode),(reason),__FILE__,__LINE__)

/* ixfr.c */
extern taskexec_t	ixfr(TASK *, datasection_t, dns_qtype_t, char *, int);
extern void		ixfr_start(void);

/* listen.c */
extern char 		**all_interface_addresses(void);
extern void		create_listeners(void);

/* main.c */
extern struct timeval	*gettick(void);
extern int		Max_FDs;
extern void		named_shutdown(int);
extern void		free_other_tasks(TASK *, int);
extern SERVER		*find_server_for_task(TASK*);
extern void		kill_server(SERVER *, int);
extern void		server_status(void);
extern void		named_cleanup(int);

/* message.c */
extern char		*dns_make_message(TASK *, uint16_t, uint8_t, dns_qtype_t,
					  char *, int , size_t *);

#define dns_make_question(t,id,qtype,name,rd,length) \
  dns_make_message((t),(id),DNS_OPCODE_QUERY,(qtype),(name),(rd),(length))
#define dns_make_notify(t,id,qtype,name,rd,length) \
  dns_make_message((t),(id),DNS_OPCODE_NOTIFY,(qtype),(name),(rd),(length))

/* notify.c */

typedef struct _notify_slave {
  int			replied;	/* Have we had a reply from the slave */
  int			retries;        /* How many retries have we made */
  time_t		lastsent;	/* Last message was sent then */
  struct sockaddr	slaveaddr;
} NOTIFYSLAVE;

extern taskexec_t	notify_write(TASK *);
extern taskexec_t	notify_read(TASK*);
extern void		notify_slaves(TASK *, MYDNS_SOA *);
extern void		notify_start(void);
extern int		name_servers2ip(TASK *, ARRAY *, ARRAY *, ARRAY *);

/* queue.c */
extern QUEUE		*queue_init(char *, char *);
extern void		queue_stats(void);
extern int		_enqueue(QUEUE **, TASK *, const char *, unsigned int);
extern void		_dequeue(QUEUE **, TASK *, const char *, unsigned int);
extern void		_requeue(QUEUE **, TASK *, const char *, unsigned int);

#define			enqueue(Q,T)	_enqueue((Q), (T), __FILE__, __LINE__)
#define			dequeue(T)	_dequeue(((T)->TaskQ), (T), __FILE__, __LINE__)
#define			requeue(Q, T)	_requeue((Q), (T), __FILE__, __LINE__)

/* recursive.c */
#if DEBUG_ENABLED
extern const char	*resolve_datasection_str[];
#endif
extern taskexec_t	recursive_fwd(TASK *);
extern taskexec_t	recursive_fwd_connect(TASK *);
extern taskexec_t	recursive_fwd_connecting(TASK *);
extern taskexec_t	recursive_fwd_write(TASK *);
extern taskexec_t	recursive_fwd_read(TASK *);


/* reply.c */
extern int		reply_init(TASK *);
extern void		abandon_reply(TASK *);
extern void		build_cache_reply(TASK *);
extern void		build_reply(TASK *, int);


/* resolve.c */
extern taskexec_t	resolve(TASK *, datasection_t, dns_qtype_t, char *, int);


/* rr.c */
extern void		rrlist_add(TASK *, datasection_t, dns_rrtype_t, void *, char *);
extern void		rrlist_free(RRLIST *);

/* servercomms.c */
extern TASK		*scomms_start(int);
extern TASK		*mcomms_start(int);

/* sort.c */
extern void		sort_a_recs(TASK *, RRLIST *, datasection_t);
extern void		sort_mx_recs(TASK *, RRLIST *, datasection_t);
extern void		sort_srv_recs(TASK *, RRLIST *, datasection_t);

/* status.c */
#if STATUS_ENABLED
extern taskexec_t	remote_status(TASK *t);
#endif

/* task.c */
extern int		task_timedout(TASK *);
extern char		*task_exec_name(taskexec_t);
extern char		*task_type_name(int);
extern char		*task_priority_name(int);
extern char		*task_string_name(TASK *);
extern TASK 		*task_find_by_id(TASK *, QUEUE *, unsigned long);
extern taskexec_t	task_new(TASK *, unsigned char *, size_t);
extern void		task_init_header(TASK *);
extern const char	*clientaddr(TASK *);
extern char		*desctask(TASK *);
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

extern void		task_build_reply(TASK *);
extern void		task_output_info(TASK *, char *);
extern int		task_process(TASK *, int, int, int);
extern void		task_start(void);


/* tcp.c */
extern int		accept_tcp_query(int, int);
extern taskexec_t	read_tcp_query(TASK *);
extern taskexec_t	write_tcp_reply(TASK *);
extern void		tcp_start(void);

/* udp.c */
extern taskexec_t	read_udp_query(int, int);
extern taskexec_t	write_udp_reply(TASK *);
extern void		udp_start(void);

/* update.c */
extern taskexec_t	dns_update(TASK *);

#endif /* _MYDNS_NAMED_H */

/* vi:set ts=3: */
