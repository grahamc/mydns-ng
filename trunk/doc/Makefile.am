##
##  $Id: Makefile.am,v 1.8 2004/02/27 18:28:06 bboy Exp $
##

man_MANS				=	mydns.8 \
							mydnscheck.8 \
							mydns.conf.5 \
							mydns-conf.8 \
							mydnsexport.8 \
							mydnsimport.8 \
							mydnsptrconvert.8

info_TEXINFOS		=	mydns.texi

mydns_TEXINFOS		=	configuration.texi \
							database.texi \
							fdl.texi \
							installation.texi \
							introduction.texi \
							references.texi \
							server.texi \
							texinfo.tex \
							troubleshooting.texi \
							utilities.texi

EXTRA_DIST			=	${man_MANS:=.in}

CLEANFILES			=	${man_MANS} variables.texi mydns.info

html:
    ##fix this code bellow
    ##if [ -x /usr/local/bin/mydns-texi2html ]; then /usr/local/bin/mydns-texi2html mydns.texi; fi


## vi:set ts=3:
