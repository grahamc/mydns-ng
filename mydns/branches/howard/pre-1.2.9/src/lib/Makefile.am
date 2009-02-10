##
##  $Id: Makefile.am,v 1.26 2005/04/20 16:43:22 bboy Exp $
##

noinst_LIBRARIES	=	libmydns.a
INCLUDES		=	@MYDNSINCLUDE@ @INTLINCLUDE@ @UTILINCLUDE@ @SQLINCLUDE@
libmydns_a_SOURCES	=	alias.c \
				cache.c conf.c \
				data.c db.c debug.c \
				encode.c error.c \
				ip.c \
				listen.c \
				message.c \
				queue.c \
				recursive.c reply.c resolve.c rr.c rrtype.c \
				server.c soa.c sql.c status.c str.c support.c \
				taskobj.c \
				unencode.c update.c

ctags:
	ctags @UTILDIR@/*.[ch] *.[ch]

##  vi:set ts=3:
