##
## $Id: Makefile.am,v 1.22 2005/04/20 16:49:12 bboy Exp $
##

bin_PROGRAMS				=	mydnscheck mydnsexport mydnsimport mydnsptrconvert mydns-conf

localedir				=	$(datadir)/locale

INCLUDES				=	@UTILINCLUDE@ @MYDNSINCLUDE@ @INTLINCLUDE@ @SQLINCLUDE@ @SSLINCLUDE@
DEFS					=	-DLOCALEDIR=\"$(localedir)\" -DSBINDIR=\"$(sbindir)\"
LDADD					=	@LIBMYDNS@ @LIBUTIL@ @LIBINTL@ @LIBSQL@ @LIBSSL@ @LIBSOCKET@ @LIBNSL@ @LIBM@

LIBDEPS					=	@LIBMYDNS@ @LIBUTIL@

mydnscheck_DEPENDENCIES			=	$(LIBDEPS)
mydnsexport_DEPENDENCIES		=	$(LIBDEPS)
mydnsimport_DEPENDENCIES		=	$(LIBDEPS)
mydnsptrconvert_DEPENDENCIES		=	$(LIBDEPS)
mydns_conf_DEPENDENCIES			=	$(LIBDEPS)

noinst_HEADERS				=	util.h

mydnscheck_SOURCES			=	check.c util.c
mydnsexport_SOURCES			=	export.c util.c
mydnsimport_SOURCES			=	import.c import-axfr.c import-tinydns.c util.c
mydnsptrconvert_SOURCES			=	libptr.c libptr.h ptrconvert.c util.c
mydns_conf_SOURCES			=	conf.c

ctags:
	ctags *.[ch] @MYDNSDIR@/*.[ch] @UTILDIR@/*.[ch]

## vi:set ts=3:
