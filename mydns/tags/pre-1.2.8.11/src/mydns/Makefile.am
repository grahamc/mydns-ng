##
##  $Id: Makefile.am,v 1.35 2005/04/20 16:49:12 bboy Exp $
##

sbin_PROGRAMS		=	mydns

localedir		=	$(datadir)/locale

INCLUDES		=	@UTILINCLUDE@ @MYDNSINCLUDE@ @INTLINCLUDE@ @SQLINCLUDE@ @SSLINCLUDE@
DEFS			=	-DLOCALEDIR=\"$(localedir)\"
LDADD			=	@LIBMYDNS@ @LIBUTIL@ @LIBINTL@ @LIBSQL@ @LIBSSL@ @LIBSOCKET@ @LIBNSL@ @LIBM@

mydns_DEPENDENCIES	=	@LIBMYDNS@ @LIBUTIL@

noinst_HEADERS		=	cache.h named.h task.h
mydns_SOURCES		=	alias.c array.c axfr.c cache.c data.c db.c encode.c \
				error.c ixfr.c listen.c main.c message.c notify.c queue.c \
				recursive.c \
				reply.c resolve.c rr.c servercomms.c sort.c status.c task.c \
				tcp.c udp.c update.c

CLEANFILES		=	malloc_trace gmon.out bb.out

ctags:
	ctags *.[ch] @MYDNSDIR@/*.[ch] @UTILDIR@/*.[ch]

## vi:set ts=3:
