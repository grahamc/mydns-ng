##
##  $Id: Makefile.am,v 1.26 2005/04/20 16:43:22 bboy Exp $
##

noinst_LIBRARIES	=	libmydns.a
INCLUDES		=	@MYDNSINCLUDE@ @INTLINCLUDE@ @UTILINCLUDE@ @SQLINCLUDE@
libmydns_a_SOURCES	=	alias.c axfr.c \
				buildreply.c \
				cache.c conf.c check.c \
				data.c db.c debug.c dnsupdate.c \
				encode.c error.c export.c \
				import.c ip.c ixfr.c \
				listen.c listeners.c \
				message.c \
				notify.c \
				queue.c \
				recursive.c reply.c resolve.c rr.c rrtype.c \
				server.c servercomms.c soa.c sort.c sql.c status.c str.c support.c \
				task.c taskobj.c tcp.c \
				udp.c unencode.c update.c util.c

ctags:
	ctags @UTILDIR@/*.[ch] *.[ch]

##  vi:set ts=3:
