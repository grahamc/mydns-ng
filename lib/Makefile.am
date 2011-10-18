##
##  $Id: Makefile.am,v 1.31 2005/04/28 18:43:14 bboy Exp $
##

EXTRA_DIST		=	README

noinst_LIBRARIES	=	libmydnsutil.a

noinst_HEADERS		=	mydnsutil.h gettext.h getopt.h

INCLUDES		=	@INTLINCLUDE@ @SQLINCLUDE@

libmydnsutil_a_SOURCES	=	alloca.c \
				cidr.c \
				conf.c \
				error.c \
				getopt.c getopt1.c \
				getoptstr.c \
				inet_pton.c \
				inet_ntop.c \
				ip.c \
				memoryman.c \
				passinput.c \
				string.c \
				strsep.c \
				strsep_quotes.c \
				wildcard.c

libmydnsutil_a_LIBADD	=	@ALLOCA@

ctags:
	ctags *.[chy] ../config.h

## vi:set ts=3:
