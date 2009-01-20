##
##  $Id: Makefile.am,v 1.26 2005/04/20 16:43:22 bboy Exp $
##

noinst_LIBRARIES	=	libmydns.a
INCLUDES		=	@MYDNSINCLUDE@ @INTLINCLUDE@ @UTILINCLUDE@ @SQLINCLUDE@
libmydns_a_SOURCES	=	conf.c check.c db.c encode.c error.c export.c \
				import.c ip.c \
				reply.c resolve.c rr.c rr_type.c \
				soa.c sql.c str.c task.c unencode.c update.c util.c

ctags:
	ctags @UTILDIR@/*.[ch] *.[ch]

##  vi:set ts=3:
