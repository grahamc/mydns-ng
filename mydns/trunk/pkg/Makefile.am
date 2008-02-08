##
##  $Id: Makefile.am,v 1.21 2005/04/20 16:49:12 bboy Exp $
##

EXTRA_DIST	=	README mydns-mysql.spec.in mydns-mysql.spec mydns-postgres.spec.in mydns-postgres.spec
CLEANFILES	=	core core* *~ mydns-mysql.spec mydns-postgres.spec *.rpm *.md5

DBLIST		=	mysql postgres

sedvars:
	if ! `test -f @SEDFILE@`; then cd $(top_srcdir) && make sedfile; fi

mydns-mysql.spec: sedvars mydns-mysql.spec.in
	sed -f @SEDFILE@ mydns-mysql.spec.in > mydns-mysql.spec

mydns-postgres.spec: sedvars mydns-postgres.spec.in
	sed -f @SEDFILE@ mydns-postgres.spec.in > mydns-postgres.spec

spec: sedvars mydns-mysql.spec mydns-postgres.spec

rpm: spec
	@if ! `test -f $(top_srcdir)/mydns-@PACKAGE_VERSION@.tar.gz`; then make -C .. dist; fi
	for db in ${DBLIST}; do \
	  RELEASE=`grep "Release:" mydns-$$db.spec | cut -f2 -d' '`; \
	  if [ ! -f $(top_srcdir)/mydns-$$db-@VERSION@-$$release.i386.rpm ]; then \
	     if [ -d mydns-@VERSION@ ]; then rm -rf mydns-@VERSION@; fi; \
	     if [ -d mydns-$$db-@VERSION@ ]; then rm -rf mydns-$$db-@VERSION@; fi; \
	     tar xzf $(top_srcdir)/mydns-@VERSION@.tar.gz; \
	     mv mydns-@VERSION@ mydns-$$db-@VERSION@; \
	     tar czf /usr/src/redhat/SOURCES/mydns-$$db-@VERSION@.tar.gz mydns-$$db-@VERSION@; \
	     rm -rf mydns-$$db-@VERSION@; \
	     cp -f mydns-$$db.spec /usr/src/redhat/SPECS; \
	     rpmbuild -ba /usr/src/redhat/SPECS/mydns-$$db.spec; \
	     cp -f /usr/src/redhat/RPMS/i386/mydns-$$db-@PACKAGE_VERSION@-$$RELEASE.i386.rpm $(top_srcdir)/pkg; \
	  fi; \
	done

release:
	for db in ${DBLIST}; do \
	  RELEASE=`grep "Release:" mydns-$$db.spec | cut -f2 -d' '`; \
	  RPM=mydns-$$db-@VERSION@-$$release.i386.rpm; \
	  if [ -f $$RPM ]; then cp -f $$RPM $(top_srcdir); fi;
	done


.PHONY: mydns-mysql.spec mydns-postgres.spec spec release rpm sedvars

## vi:set ts=3:
