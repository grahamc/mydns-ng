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
extern char *datasection_str[];			/* Strings describing data section types */
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
/* lib/alias.c */
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

/* lib/check.c */
extern void __mydns_rrproblem(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			      const char *fmt, ...) __printflike(5,6);
extern void __mydns_check_name_extended(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
					const char *name_in, size_t namelen_in, const char *fqdn, const char *col);
extern void __mydns_check_name(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			       const char *name_in, size_t namelen_in, const char *col, int is_rr, int allow_underscore);
extern void __mydns_check_rr_a(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
			       const char *a_in, size_t alen_in);
extern void __mydns_check_rr_aaaa(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
				  const char *aaaa_in, size_t aaaalen_in);
extern void __mydns_check_rr_cname(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
				   const char *cname_in, size_t cnamelen_in);
extern void __mydns_check_rr_hinfo(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
				   const char *hinfo_in, size_t hinfolen_in);
extern void __mydns_check_rr_mx(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
				const char *mx_in, size_t mxlen_in);
extern void __mydns_check_rr_naptr(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
				   const char *naptr_in, size_t naptrlen_int);
extern void __mydns_check_rr_ns(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
				const char *ns_in, size_t nslen_in);
extern void __mydns_check_rr_rp(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
				const char *rp_in, size_t rplen_in);
extern void __mydns_check_rr_srv(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
				 const char *srv_in, size_t srvlen_in);
extern void __mydns_check_rr_txt(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
				 const char *txt_in, size_t txtlen_in);
extern void __mydns_check_rr_unknown(MYDNS_SOA *soa, MYDNS_RR *rr, const char *name, char *data,
				     const char *unknown_in, size_t unknownlen_in);

/* conf.c */
extern void		load_config(void);
extern void		dump_config(void);
extern void		conf_set_logging(void);
extern void		check_config_file_perms(void);


/* data.c */
extern MYDNS_SOA	*find_soa(TASK *, char *, char **);
extern MYDNS_RR		*find_rr(TASK *, MYDNS_SOA *, dns_qtype_t, char *);


/* lib/encode.c */
extern int		name_remember(TASK *, char *, unsigned int);
extern void		name_forget(TASK *);
extern unsigned int	name_find(TASK *, char *);
extern int		name_encode(TASK *, char **, char *, unsigned int, int);


/* lib/error.c */
extern taskexec_t	_formerr_internal(TASK *, dns_rcode_t, task_error_t, char *, const char *, unsigned int);
extern taskexec_t	_dnserror_internal(TASK *, dns_rcode_t, task_error_t, const char *, unsigned int);
extern char		*err_reason_str(TASK *, task_error_t);
extern int		rr_error_repeat(uint32_t);
extern int		rr_error(uint32_t, const char *, ...) __printflike(2,3);

#define formerr(task,rcode,reason,xtra)	_formerr_internal((task),(rcode),(reason),(xtra),__FILE__,__LINE__)
#define dnserror(task,rcode,reason)			_dnserror_internal((task),(rcode),(reason),__FILE__,__LINE__)

/* lib/export.c */
void __mydns_bind_dump_rr_unknown(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				  size_t datalen, int ttl, int aux, int maxlen);
void __mydns_bind_dump_rr_default(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				  size_t datalen, int ttl, int aux, int maxlen);
void __mydns_bind_dump_rr_mx(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			     size_t datalen, int ttl, int aux, int maxlen);
void __mydns_bind_dump_rr_rp(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			     size_t datalen, int ttl, int aux, int maxlen);
void __mydns_bind_dump_rr_srv(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			      size_t datalen, int ttl, int aux, int maxlen);
void __mydns_bind_dump_rr_txt(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			      size_t datalen, int ttl, int aux, int maxlen);

void __mydns_tinydns_dump_rr_unknown(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				     size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_a(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
			       size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_cname(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				   size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_mx(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_ns(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_srv(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				 size_t datalen, int ttl, int aux);
void __mydns_tinydns_dump_rr_txt(MYDNS_SOA *soa, MYDNS_RR *rr, char *name, char *data,
				 size_t datalen, int ttl, int aux);

/* ixfr.c */
extern taskexec_t	ixfr(TASK *, datasection_t, dns_qtype_t, char *, int);
extern void		ixfr_start();

/* lib/import.c */
extern char *__mydns_process_axfr_default(char *rv, char *name, char *origin,
					  char *reply, size_t replylen, char *src, uint32_t ttl,
					  dns_qtype_map *map);
extern char *__mydns_process_axfr_a(char *rv, char *name, char *origin,
				    char *reply, size_t replylen, char *src, uint32_t ttl,
				    dns_qtype_map *map);
extern char *__mydns_process_axfr_aaaa(char *rv, char *name, char *origin,
				       char *reply, size_t replylen, char *src, uint32_t ttl,
				       dns_qtype_map *map);
extern char *__mydns_process_axfr_cname(char *rv, char *name, char *origin,
					char *reply, size_t replylen, char *src, uint32_t ttl,
					dns_qtype_map *map);
extern char *__mydns_process_axfr_hinfo(char *rv, char *name, char *origin,
					char *reply, size_t replylen, char *src, uint32_t ttl,
					dns_qtype_map *map);
extern char *__mydns_process_axfr_mx(char *rv, char *name, char *origin,
				     char *reply, size_t replylen, char *src, uint32_t ttl,
				     dns_qtype_map *map);
extern char *__mydns_process_axfr_ns(char *rv, char *name, char *origin,
				     char *reply, size_t replylen, char *src, uint32_t ttl,
				     dns_qtype_map *map);
extern char *__mydns_process_axfr_ptr(char *rv, char *name, char *origin,
				      char *reply, size_t replylen, char *src, uint32_t ttl,
				      dns_qtype_map *map);
extern char *__mydns_process_axfr_rp(char *rv, char *name, char *origin,
				     char *reply, size_t replylen, char *src, uint32_t ttl,
				     dns_qtype_map *map);
extern char *__mydns_process_axfr_soa(char *rv, char *name, char *origin,
				      char *reply, size_t replylen, char *src, uint32_t ttl,
				      dns_qtype_map *map);
extern char *__mydns_process_axfr_srv(char *rv, char *name, char *origin,
				      char *reply, size_t replylen, char *src, uint32_t ttl,
				      dns_qtype_map *map);
extern char *__mydns_process_axfr_txt(char *rv, char *name, char *origin,
				      char *reply, size_t replylen, char *src, uint32_t ttl,
				      dns_qtype_map *map);

/* main.c */
extern struct timeval	*gettick();
extern int		Max_FDs;
extern void		named_shutdown(int);
extern void		free_other_tasks(TASK *, int);
extern SERVER		*find_server_for_task(TASK*);
extern void		kill_server(SERVER *, int);

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
extern void		notify_start();
extern int		name_servers2ip(TASK *, MYDNS_SOA *, ARRAY *, ARRAY *, ARRAY *);

/* queue.c */
extern QUEUE		*queue_init(char *, char *);
extern void		queue_stats();
extern int		_enqueue(QUEUE **, TASK *, const char *, unsigned int);
extern void		_dequeue(QUEUE **, TASK *, const char *, unsigned int);
extern void		_requeue(QUEUE **, TASK *, const char *, unsigned int);

#define			enqueue(Q,T)	_enqueue((Q), (T), __FILE__, __LINE__)
#define			dequeue(T)	_dequeue(((T)->TaskQ), (T), __FILE__, __LINE__)
#define			requeue(Q, T)	_requeue((Q), (T), __FILE__, __LINE__)

/* recursive.c */
#if DEBUG_ENABLED
extern char		*resolve_datasection_str[];
#endif
extern taskexec_t	recursive_fwd(TASK *);
extern taskexec_t	recursive_fwd_connect(TASK *);
extern taskexec_t	recursive_fwd_connecting(TASK *);
extern taskexec_t	recursive_fwd_write(TASK *);
extern taskexec_t	recursive_fwd_read(TASK *);


/* lib/reply.c */
extern void		mydns_reply_add_additional(TASK *t, RRLIST *rrlist, datasection_t section);
extern int		__mydns_reply_add_generic_rr(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_a(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_aaaa(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_hinfo(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_mx(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_naptr(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_rp(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_soa(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_srv(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_add_txt(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_unexpected_type(TASK *t, RR *r, dns_qtype_map *map);
extern int		__mydns_reply_unknown_type(TASK *t, RR *r, dns_qtype_map *map);


/* reply.c */
extern int		reply_init(TASK *);
extern void		abandon_reply(TASK *);
extern void		build_cache_reply(TASK *);
extern void		build_reply(TASK *, int);


/* resolve.c */
extern taskexec_t	resolve(TASK *, datasection_t, dns_qtype_t, char *, int);


/* lib/rr.c */
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

/* support.c */
extern int 	shutting_down;		/* Shutdown in progress? */
extern void	named_cleanup(int signo);
extern char	*mydns_name_2_shortname(char *name, char *origin, int empty_name_is_ok, int notrim);

/* lib/task.c */
extern uint8_t		*taskvec;
extern uint16_t		internal_id;
extern uint32_t 	answer_then_quit;		/* Answer this many queries then quit */

extern char		*task_exec_name(taskexec_t);
extern char		*task_type_name(int);
extern char		*task_priority_name(int);
extern char		*task_string_name(TASK *);
extern const char	*clientaddr(TASK *);
extern char		*desctask(TASK *);

/* task.c */
extern int		task_timedout(TASK *);
extern TASK 		*task_find_by_id(TASK *, QUEUE *, unsigned long);
extern taskexec_t	task_new(TASK *, unsigned char *, size_t);
extern void		task_init_header(TASK *);
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
extern void		task_start();


/* tcp.c */
extern int		accept_tcp_query(int, int);
extern taskexec_t	read_tcp_query(TASK *);
extern taskexec_t	write_tcp_reply(TASK *);
extern void		tcp_start();

/* udp.c */
extern taskexec_t	read_udp_query(int, int);
extern taskexec_t	write_udp_reply(TASK *);
extern void		udp_start();

/* lib/update.c */
extern taskexec_t __mydns_update_get_rr_data_unknown_type(TASK *t,
							  UQRR *rr,
							  char **data,
							  size_t *datalen,
							  char **edata,
							  size_t *edatalen,
							  uint32_t *aux,
							  dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_a(TASK *t,
					       UQRR *rr,
					       char **data,
					       size_t *datalen,
					       char **edata,
					       size_t *edatalen,
					       uint32_t *aux,
					       dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_aaaa(TASK *t,
						  UQRR *rr,
						  char **data,
						  size_t *datalen,
						  char **edata,
						  size_t *edatalen,
						  uint32_t *aux,
						  dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_cname(TASK *t,
						   UQRR *rr,
						   char **data,
						   size_t *datalen,
						   char **edata,
						   size_t *edatalen,
						   uint32_t *aux,
						   dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_hinfo(TASK *t,
						   UQRR *rr,
						   char **data,
						   size_t *datalen,
						   char **edata,
						   size_t *edatalen,
						   uint32_t *aux,
						   dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_mx(TASK *t,
						UQRR *rr,
						char **data,
						size_t *datalen,
						char **edata,
						size_t *edatalen,
						uint32_t *aux,
						dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_ns(TASK *t,
						UQRR *rr,
						char **data,
						size_t *datalen,
						char **edata,
						size_t *edatalen,
						uint32_t *aux,
						dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_ptr(TASK *t,
						 UQRR *rr,
						 char **data,
						 size_t *datalen,
						 char **edata,
						 size_t *edatalen,
						 uint32_t *aux,
						 dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_rp(TASK *t,
						UQRR *rr,
						char **data,
						size_t *datalen,
						char **edata,
						size_t *edatalen,
						uint32_t *aux,
						dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_srv(TASK *t,
						 UQRR *rr,
						 char **data,
						 size_t *datalen,
						 char **edata,
						 size_t *edatalen,
						 uint32_t *aux,
						 dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_txt(TASK *t,
						 UQRR *rr,
						 char **data,
						 size_t *datalen,
						 char **edata,
						 size_t *edatalen,
						 uint32_t *aux,
						 dns_qtype_map *map);

/* update.c */
extern taskexec_t	dns_update(TASK *);

#endif /* _MYDNS_NAMED_H */

/* vi:set ts=3: */
