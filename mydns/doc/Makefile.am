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

sedfile:
	if ! test -f @SEDFILE@; then \
		cd @top_srcdir@ && $(MAKE) sedfile; \
	fi

mydns.8: sedfile ${@:=.in}
	$(SHELL) @top_srcdir@/missing --run sed -f @SEDFILE@ ${@:=.in} | tr -s '\n' > $@

mydnscheck.8: sedfile ${@:=.in}
	$(SHELL) @top_srcdir@/missing --run sed -f @SEDFILE@ ${@:=.in} | tr -s '\n' > $@

mydns.conf.5: sedfile ${@:=.in}
	$(SHELL) @top_srcdir@/missing --run sed -f @SEDFILE@ ${@:=.in} | tr -s '\n' > $@

mydns-conf.8: sedfile ${@:=.in}
	$(SHELL) @top_srcdir@/missing --run sed -f @SEDFILE@ ${@:=.in} | tr -s '\n' > $@

mydnsexport.8: sedfile ${@:=.in}
	$(SHELL) @top_srcdir@/missing --run sed -f @SEDFILE@ ${@:=.in} | tr -s '\n' > $@

mydnsimport.8: sedfile ${@:=.in}
	$(SHELL) @top_srcdir@/missing --run sed -f @SEDFILE@ ${@:=.in} | tr -s '\n' > $@

mydnsptrconvert.8: sedfile ${@:=.in}
	$(SHELL) @top_srcdir@/missing --run sed -f @SEDFILE@ ${@:=.in} | tr -s '\n' > $@

html:
	if [ -x /usr/local/bin/mydns-texi2html ]; then /usr/local/bin/mydns-texi2html mydns.texi; fi

.PHONY: sedfile

## vi:set ts=3:
