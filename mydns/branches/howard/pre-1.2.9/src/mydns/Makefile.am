##
##  $Id: Makefile.am,v 1.35 2005/04/20 16:49:12 bboy Exp $
##

sbin_PROGRAMS		=	mydns

localedir		=	$(datadir)/locale

INCLUDES		=	@UTILINCLUDE@ @MYDNSINCLUDE@ @INTLINCLUDE@ @SQLINCLUDE@ @SSLINCLUDE@
DEFS			=	-DLOCALEDIR=\"$(localedir)\"
LDADD			=	@LIBMYDNS@ @LIBUTIL@ @LIBINTL@ @LIBSQL@ @LIBSSL@ @LIBSOCKET@ @LIBNSL@ @LIBM@

mydns_DEPENDENCIES	=	@LIBMYDNS@ @LIBUTIL@

noinst_HEADERS		=	
mydns_SOURCES		=	main.c

CLEANFILES		=	malloc_trace gmon.out bb.out

ctags:
	ctags *.[ch] @MYDNSDIR@/*.[ch] @UTILDIR@/*.[ch]

## vi:set ts=3:
