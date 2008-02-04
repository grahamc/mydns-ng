##
## $Id: Makefile.am,v 1.48 2006/01/18 22:02:16 bboy Exp $
##

SUBDIRS				=	pkg contrib intl po m4 doc lib src

SEDFILE				=	version.sed
ACLOCAL_AMFLAGS	=	-I m4

EXTRA_DIST			=	BUGS config.rpath makeinfo QUICKSTART.mysql QUICKSTART.postgres README.mysql TODO

CLEANFILES			=	core* *~ $(SEDFILE) gmon.out bb.out @PACKAGE_NAME@-*.gz @PACKAGE_NAME@-*.bz2

## Creates a small .sed script to replace various variables
## (Used by doc and pkg)
sedfile:
	@if `test -f $(SEDFILE)`; then rm -f $(SEDFILE); fi
	@echo "s,@MYDNS_CONF\@,@MYDNS_CONF@,g" >> $(SEDFILE)
	@echo "s,@PACKAGE_DATE\@,@PACKAGE_DATE@,g" >> $(SEDFILE)
	@echo "s,@PACKAGE_NAME\@,@PACKAGE_NAME@,g" >> $(SEDFILE)
	@echo "s,@PACKAGE_VERSION\@,@PACKAGE_VERSION@,g" >> $(SEDFILE)
	@echo "s,@PACKAGE_STRING\@,@PACKAGE_STRING@,g" >> $(SEDFILE)
	@echo "s,@PACKAGE_AUTHOR\@,@PACKAGE_AUTHOR@,g" >> $(SEDFILE)
	@echo "s,@PACKAGE_BUGREPORT\@,@PACKAGE_BUGREPORT@,g" >> $(SEDFILE)
	@echo "s,@PACKAGE_HOMEPAGE\@,@PACKAGE_HOMEPAGE@,g" >> $(SEDFILE)
	@echo "s,@PACKAGE_COPYRIGHT\@,@PACKAGE_COPYRIGHT@,g" >> $(SEDFILE)

install-data-hook:
	@echo
	@echo "  ###"
	@echo "  ###  @PACKAGE_NAME@ version @PACKAGE_VERSION@ installed!"
	@echo "  ###"
	@echo "  ###  See the file QUICKSTART if you are in a hurry."
	@echo "  ###  Otherwise, consult the manual (in doc/)."
	@echo "  ###"
	@echo "  ###  The MyDNS server program was installed as"
	@echo "  ###  @sbindir@/mydns"
	@echo "  ###"
	@echo "  ###  Your configuration file should be installed as"
	@echo "  ###  @MYDNS_CONF@"
	@if ! test -f @MYDNS_CONF@; then echo "  ###  (You can auto-create it with \"make conf\")"; fi
	@echo "  ###"
	@echo

## The `conf' target creates an /etc/mydns.conf template
conf:
	@if ! test -x src/mydns/mydns; then \
	   echo; echo "  ###"; \
	   echo "  ###  MyDNS binary not found - please compile MyDNS first"; \
	   echo "  ###  by running \"make\"."; \
	   echo "  ###"; echo; \
	else \
	   if test -f @MYDNS_CONF@; then \
	      echo; echo "  ###"; \
	      echo "  ###  @MYDNS_CONF@ already exists!  Nothing done."; \
	      echo "  ###"; echo; \
	   else \
		   src/mydns/mydns --dump-config > @MYDNS_CONF@; \
		   chmod 0600 @MYDNS_CONF@; \
	      echo; echo "  ###"; \
		   echo "  ###  @MYDNS_CONF@ created.  See mydns.conf(5) for details."; \
	      echo "  ###"; echo; \
	   fi; \
	fi

## The `localconf' target just outputs a mydns.conf template
localconf:
	@if ! test -x src/mydns/mydns; then \
	   echo; echo "  ###"; \
	   echo "  ###  MyDNS binary not found - please compile MyDNS first"; \
	   echo "  ###  by running \"make\"."; \
	   echo "  ###"; echo; \
	else \
	   src/mydns/mydns --dump-config; \
	fi

news:
	@if [ -x ./mknews ]; then ./mknews > NEWS; cat NEWS; fi

dist-hook: news
	rm -rf `find $(distdir)/doc/nls -name CVS`

distready: news
	rm -rf mydns-*
	po/mkpotfile

release: clean distready
	chown -R 0:0 .
	if [ -x ./release.sh ]; then ./release.sh $(VERSION) `pwd`; fi

.PHONY: release distready news localconf conf sedfile

# vi:set ts=3:
