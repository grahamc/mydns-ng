##
##	$Id: configure.ac,v 1.114 2005/04/21 16:40:13 bboy Exp $
##
##	Copyright (C) 2002-2005  Don Moore <bboy@bboy.net>
##
##	This program is free software; you can redistribute it and/or modify
##	it under the terms of the GNU General Public License as published by
##	the Free Software Foundation; either version 2 of the License, or
##	(at Your option) any later version.
##
##	This program is distributed in the hope that it will be useful,
##	but WITHOUT ANY WARRANTY; without even the implied warranty of
##	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##	GNU General Public License for more details.
##
##	You should have received a copy of the GNU General Public License
##	along with this program; if not, write to the Free Software
##	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##

AC_INIT(mydns, [1.2.8.13], [support@cohtech.com])	## Package name, version, bugreport address
AC_PREREQ([2.59])
AC_COPYRIGHT([Portions of this script are Copyright 2002-2005 Don Moore  2007-2008 Howard Wilkinson])
AM_INIT_AUTOMAKE([1.9.5 dist-bzip2])
AM_CONFIG_HEADER(config.h)

## Checks for programs
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL

## GNU gettext
AM_GNU_GETTEXT
AM_GNU_GETTEXT_VERSION

## Checks for header files
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h getopt.h libintl.h netdb.h pwd.h signal.h stdarg.h termios.h time.h])
AC_CHECK_HEADERS([sys/fcntl.h sys/file.h sys/ioctl.h sys/resource.h])
AC_CHECK_HEADERS([sys/socket.h sys/sockio.h sys/time.h sys/wait.h])
AC_CHECK_HEADERS([sys/select.h])
AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([net/if.h])
AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([poll.h])

## Typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_CHECK_TYPES([uchar,uint,uint8_t,uint16_t,uint32_t])

## Library functions
AC_FUNC_MALLOC
AC_TYPE_SIGNAL
AC_FUNC_ALLOCA
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset getpwuid shutdown inet_ntop inet_pton strsep strndup strdup])
AC_CHECK_FUNCS([select poll])
AC_CHECK_FUNCS([getaddrinfo getnameinfo])
AC_LIB_SOCKET
AC_LIB_NSL
AC_LIB_MATH

## Declarations
AC_CHECK_DECLS([strcasecmp, strcoll, strerror, strncasecmp, strsep, strndup, strdup])
AC_CHECK_DECLS([poll, select])


##
## Package-specific variables
##
PACKAGE_HOMEPAGE="http://mydns.bboy.net"
PACKAGE_COPYRIGHT="Copyright (C) 2002-2005 Don Moore  2007-2008 Howard Wilkinson"
PACKAGE_AUTHOR="Don Moore <bboy@bboy.net>  Howard Wilkinson <howard@cohtech.com>"


##
## Some MyDNS-specific checks (see m4/mydns.m4)
##
AC_CHECK_IPV6			#	Check IPv6 support
AC_CHECK_SOCKADDR_SA_LEN	#	Check for sa_len in struct sockaddr_in
AC_MYDNS_PKGINFO		#	Set some package-specific variables
AC_ENABLE_ALIAS			#	Enable David Phillips aliasing?
AC_CHECK_MYSQL			#	Check for MySQL support
AC_CHECK_PGSQL			#	Check for PostgreSQL support
AC_WITH_OPENSSL			#	Check if OpenSSL is desired
AC_MYDNS_CONF			#	Set conf file location
AC_ENABLE_STATIC_BUILD		#	Build static binary?
AC_CHOOSE_DB			#	Choose between MySQL and PostgreSQL
AC_ENABLE_PROFILING		#	Enable profiling?
AC_ENABLE_DEBUG			#	Enable debugging?
AC_ENABLE_VALGRIND		#	Compile with Valgrind-friendly flags?
AC_ENABLE_STRICT		#	Compile with extra-strict warnings?
AC_ENABLE_ALT_NAMES		#	Alternate column names for DN
AC_ENABLE_STATUS		#	Enable the STATUS opcode to check server status
AC_DISABLE_DATE_LOGGING		#	Disable date/time in verbose output?
AC_MYDNS_VARS			#  Set variables containing dirs

AC_OUTPUT(	contrib/Makefile \
		intl/Makefile \
		po/Makefile.in \
		m4/Makefile \
		Makefile \
		lib/Makefile \
		src/Makefile \
		src/lib/Makefile \
		src/mydns/Makefile \
		src/util/Makefile \
		pkg/Makefile \
		doc/Makefile
	  )

echo
echo "  ###"
echo "  ###  MyDNS $PACKAGE_VERSION configured.  Using $USE_DB_NAME database."
echo "  ###"
echo "  ###  Type \"make\" to build, then \"make install\" to install."
echo "  ###"
echo

## vi:set ts=3:
