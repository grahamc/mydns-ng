##
##	mydns.m4
## autoconf macros for the MyDNS package.
##
##	Copyright (C) 2002-2005  Don Moore <bboy@bboy.net>
##
##	$Id: mydns.m4,v 1.76 2006/01/18 22:05:42 bboy Exp $
##

## OpenSSL include check paths:
## /usr/ssl/include /usr/local/ssl/include /usr/include /usr/include/ssl /opt/ssl/include /opt/openssl/include /usr/local/ssl/include /usr/local/include

## OpenSSL lib check paths:
## /usr/ssl/lib /usr/local/ssl/lib /usr/lib/openssl /usr/lib /opt/ssl/lib /opt/openssl/lib /usr/local/lib/

##
## Check for cflags
##
AC_DEFUN([AC_C_CFLAGS],
	[
		ac_save_CFLAGS=$CFLAGS
		CFLAGS="$1"
		AC_MSG_CHECKING(whether $CC accepts $1)
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [ac_cv_prog_cc_optok=yes], [ac_cv_prog_cc_optok=no])
		if test "$ac_cv_prog_cc_optok" = "yes"; then
			AC_MSG_RESULT([yes])
			CFLAGS="$ac_save_CFLAGS $1"
		else
			AC_MSG_RESULT([no])
			CFLAGS=$ac_save_CFLAGS
		fi
	]
)

##
##	Locate OpenSSL libraries (LIBSSL)
##
AC_DEFUN([AC_WITH_OPENSSL_LIB],
	[
		ac_ssl_lib_dirs="/lib /usr/lib /usr/local/lib /usr/ssl/lib /usr/local/ssl/lib"
		AC_ARG_WITH(openssl-lib,
			AC_HELP_STRING([--with-openssl-lib=DIR], [look for the OpenSSL libraries in DIR]),
			ac_ssl_lib_dirs="$withval $ac_ssl_lib_dirs")
		ac_ssl_lib_found=no
		ac_mydns_libs="$LDFLAGS"
		for dir in $ac_ssl_lib_dirs
		do
			if test "$ac_ssl_lib_found" != yes
			then
				AC_CHECK_FILE($dir/libssl.a, ac_ssl_lib_found=yes, ac_ssl_lib_found=no)
				if test "$ac_ssl_lib_found" = yes
				then
					LIBSSL="-L$dir -lssl -lcrypto"
				fi
			fi
		done
		if test "$ac_ssl_lib_found" != yes
		then
			AC_MSG_ERROR([

][  ###
][  ###  OpenSSL libraries (libssl.a/libcrypto.a) not found!
][  ###
][  ###  If your OpenSSL libraries are in an unusual location,
][  ###  specify the directory where they are located with:
][  ###
][  ###       --with-openssl-lib=DIR
][  ###
][  ###  If you don't have the OpenSSL development files on your
][  ###  system, download and install them from the following URL:
][  ###
][  ###       http://www.openssl.org/
][  ###
][  ###  (Error detail might be available in `config.log')
][  ###
])
		fi
		AC_SUBST(LIBSSL)
	]
)

##
##	Locate OpenSSL header files (SSLINCLUDE)
##
AC_DEFUN([AC_WITH_OPENSSL_INCLUDE],
	[
		ac_ssl_header_dirs="/usr/include /usr/local/include /usr/include/ssl /usr/ssl/include /usr/local/ssl/include"
		ac_ssl_header_found=no, ac_ssl_header_ok=no
		AC_ARG_WITH(openssl-include,
			AC_HELP_STRING([--with-openssl-include=DIR], [look for OpenSSL include files in DIR]),
			ac_ssl_header_dirs="$withval $ac_ssl_header_dirs")
		for dir in $ac_ssl_header_dirs
		do
			if test "$ac_ssl_header_found" != yes
			then
				AC_CHECK_FILE($dir/openssl/ssl.h, ac_ssl_header_found=yes, ac_ssl_header_found=no)
				test "$ac_ssl_header_found" = yes && SSL_INCLUDE="-I$dir"
			fi
		done
		if test "$ac_ssl_header_found" != yes
		then
			AC_MSG_ERROR([

][  ###
][  ###  OpenSSL header file (openssl/ssl.h) not found!
][  ###
][  ###  If your OpenSSL header files are in an unusual location,
][  ###  specify the directory where they are located with:
][  ###
][  ###       --with-openssl-include=DIR
][  ###
][  ###  If you don't have the OpenSSL development files on your
][  ###  system, download and install them from the following URL:
][  ###
][  ###       http://www.openssl.org/
][  ###
][  ###  (Error detail might be available in `config.log')
][  ###
])

		fi
		AC_DEFINE([WITH_SSL], 1, [SSL support enabled?])
		AC_SUBST(SSLINCLUDE)
	]
)


##
##	Build with OpenSSL support?  (needed by some MySQL installations)
##
AC_DEFUN([AC_WITH_OPENSSL],
	[
		AC_MSG_CHECKING([whether to compile with OpenSSL support])
		AC_ARG_WITH(openssl,
			AC_HELP_STRING([--with-openssl], [compile with OpenSSL support?]),
			[
				if test "$withval" = "yes"; then
					AC_MSG_RESULT([yes])
					AC_WITH_OPENSSL_LIB
					AC_WITH_OPENSSL_INCLUDE
				else
					AC_MSG_RESULT([no])
				fi
			], [
				AC_MSG_RESULT([no])
			]
		)
	]
)


##
## Build static binary
##
AC_DEFUN([AC_ENABLE_STATIC_BUILD],
	[
		AC_MSG_CHECKING([whether to build static binary])
		AC_ARG_ENABLE(static-build,
			AC_HELP_STRING([--enable-static-build], [build a static binary (mainly for RPM)]),
			[
				if test "$enableval" = yes
				then
					AC_MSG_RESULT([yes])
					LDFLAGS="$LDFLAGS -all-static"
					ac_mydns_static_build=yes
					AC_SUBST(ac_mydns_static_build)

					## PostgreSQL client library needs libcrypt if statically compiled
					if test -n "$LIBPQ"; then
						LIBPQ="$LIBPQ -lcrypt"
					fi

				else
					AC_MSG_RESULT([no])
				fi
			], AC_MSG_RESULT([no]))
	]
)


##
## Disable date logging?
##
AC_DEFUN([AC_DISABLE_DATE_LOGGING],
	[
		AC_MSG_CHECKING([whether to disable date/time in verbose output])
		AC_ARG_ENABLE(date-logging,
			AC_HELP_STRING([--disable-date-logging], [disable date/time in verbose output]),
			[
				if test "$enableval" = yes
				then
					AC_MSG_RESULT([no])
					AC_DEFINE(DISABLE_DATE_LOGGING, 0, [Disable date/time in verbose output?])
				else
					AC_MSG_RESULT([yes])
					AC_DEFINE(DISABLE_DATE_LOGGING, 1, [Disable date/time in verbose output?])
				fi
			], [
				AC_MSG_RESULT([no])
				AC_DEFINE(DISABLE_DATE_LOGGING, 0, [Disable date/time in verbose output?])
			])
	]
)

##
## Check for IPv6 support
##
AC_DEFUN([AC_CHECK_IPV6],
	[
		AC_MSG_CHECKING([whether IPv6 is supported])
		AC_TRY_COMPILE(
			[#include <netinet/in.h>],
			[struct in6_addr addr; memset(&addr,0,sizeof(struct in6_addr));],
				[ AC_DEFINE([HAVE_IPV6], 1, [Is IPv6 supported?])
				  AC_MSG_RESULT([yes]) ],
				AC_MSG_RESULT([no]))
	]
)

 
##
## Check for `sa_len' in `struct sockaddr'
##
AC_DEFUN([AC_CHECK_SOCKADDR_SA_LEN],
	[
		AC_MSG_CHECKING([for sa_len in struct sockaddr])
		AC_TRY_COMPILE([
#if HAVE_SYS_TYPES_H
#	include <sys/types.h>
#endif

#if HAVE_SYS_SOCKET_H
#	include <sys/socket.h>
#endif
], [void *p = &((struct sockaddr *)0L)->sa_len;],
		[ AC_DEFINE(HAVE_SOCKADDR_SA_LEN, 1, [Does struct sockaddr contain sa_len?])
		  AC_MSG_RESULT([yes]) ],
		AC_MSG_RESULT([no]))
	]
)


##
##  Compile for profiling?
##
AC_DEFUN([AC_ENABLE_PROFILING],
	[
		AC_MSG_CHECKING([whether to enable profiling])
		AC_ARG_ENABLE(profiling,
			AC_HELP_STRING([--enable-profiling], [enable profiling]),
			[
				if test "$enableval" = yes
				then
					AC_DEFINE(PROFILING, 1, [Compile with support for profiling?])
					AC_MSG_RESULT([yes])
					CFLAGS="-O -g -pg"
					LDFLAGS="$LDFLAGS -pg"
					PROFILE_ENABLED=yes
					AC_SUBST(PROFILE_ENABLED)
					AC_C_CFLAGS(-fno-inline-functions)
					AC_C_CFLAGS(-ffast-math)
				else
					AC_MSG_RESULT([no])
				fi
			], [
				AC_MSG_RESULT([no])
			]
		)
	]
)


##
##  Compile for valgrinding?
##
AC_DEFUN([AC_ENABLE_VALGRIND],
	[
		AC_MSG_CHECKING([whether to compile with Valgrind-friendly flags])
		AC_ARG_ENABLE(valgrind,
			AC_HELP_STRING([--enable-valgrind], [compile with Valgrind-friendly flags]),
			[
				if test "$enableval" = yes
				then
					AC_MSG_RESULT([yes])
					CFLAGS="-O -g"
					AC_C_CFLAGS(-fno-inline)
					AC_C_CFLAGS(-ffast-math)
					AC_C_CFLAGS(-funsigned-char)
				else
					AC_MSG_RESULT([no])
				fi
			], [
				AC_MSG_RESULT([no])
			]
		)
	]
)


##
##  Compile with strict warnings
##
AC_DEFUN([AC_ENABLE_STRICT],
	[
		AC_MSG_CHECKING([whether to compile with strict warning flags])
		AC_ARG_ENABLE(strict-warnings,
			AC_HELP_STRING([--enable-strict-warnings], [compile with strict warning flags]),
			[
				if test "$enableval" = yes
				then
					AC_MSG_RESULT([yes])
					CFLAGS="-O -g"
					AC_C_CFLAGS(-W)
					AC_C_CFLAGS(-Wall)
#					AC_C_CFLAGS(-Wundef)
					AC_C_CFLAGS(-Wendif-labels)
					AC_C_CFLAGS(-Wshadow)
					AC_C_CFLAGS(-Wpointer-arith)
					AC_C_CFLAGS(-Wcast-align)
					AC_C_CFLAGS(-Wwrite-strings)
					AC_C_CFLAGS(-Wstrict-prototypes)
					AC_C_CFLAGS(-Wmissing-prototypes)
					AC_C_CFLAGS(-Wnested-externs)
					AC_C_CFLAGS(-Winline)
					AC_C_CFLAGS(-Wdisabled-optimization)
#					AC_C_CFLAGS(-std=c89)
#					AC_C_CFLAGS(-pedantic)
#					AC_C_CFLAGS(-Werror)
					AC_C_CFLAGS(-ffast-math)
					AC_C_CFLAGS(-funsigned-char)
				else
					AC_MSG_RESULT([no])
				fi
			], [
				AC_MSG_RESULT([no])
			]
		)
	]
)

##
##  Should debug be enabled?
##
AC_DEFUN([AC_ENABLE_DEBUG],
	[
		AC_MSG_CHECKING([whether to enable debugging])
		AC_ARG_ENABLE(debug,
			AC_HELP_STRING([--enable-debug], [enable debugging options]),
			[
				if test "$enableval" = yes
				then
					CFLAGS="-O -g"
					AC_DEFINE(DEBUG_ENABLED, 1, [Compile with support for debugging options?])
					AC_MSG_RESULT([yes])
					AC_C_CFLAGS(-fno-inline)
					AC_C_CFLAGS(-ffast-math)
					AC_C_CFLAGS(-Wall)
					AC_C_CFLAGS(-Wno-unused)
					AC_C_CFLAGS(-Werror)
				else
					AC_MSG_RESULT([no])
					if test -z "$PROFILE_ENABLED"; then
						AC_C_CFLAGS(-fomit-frame-pointer)
						AC_C_CFLAGS(-finline-functions)
						AC_C_CFLAGS(-ffast-math)
						LDFLAGS="$LDFLAGS -static"
					fi
				fi
			], [
				AC_MSG_RESULT([no])
				if test -z "$PROFILE_ENABLED"; then
					AC_C_CFLAGS(-fomit-frame-pointer)
					AC_C_CFLAGS(-finline-functions)
					AC_C_CFLAGS(-ffast-math)
					LDFLAGS="$LDFLAGS -static"
				fi
			]
		)
		AC_C_CFLAGS(-funsigned-char)

		AC_MSG_CHECKING([CFLAGS])
		AC_MSG_RESULT([$CFLAGS])
	]
)


##
##	Make --enable-alias turn on alias patch
##
AC_DEFUN([AC_ENABLE_ALIAS],
	[
		AC_MSG_CHECKING([whether to enable alias patch])
		AC_ARG_ENABLE(alias,
			AC_HELP_STRING([--enable-alias], [enable alias patch (see contrib/README.alias)]),
			[
				if test "$enableval" = yes
				then
					AC_DEFINE(ALIAS_ENABLED, 1, [Compile with support for alias patch?])
					AC_MSG_RESULT([yes])
				else
					AC_MSG_RESULT([no])
				fi
			], AC_MSG_RESULT([no]))
	]
)


##
##	Make --enable-alt-names turn on alternate column names for DN
##
AC_DEFUN([AC_ENABLE_ALT_NAMES],
	[
		AC_MSG_CHECKING([whether to enable alternate column names])
		AC_ARG_ENABLE(alt-names,
			AC_HELP_STRING([--enable-alt-names], [enable alternate column names (do not use)]),
			[
				if test "$enableval" = yes
				then
					AC_DEFINE(DN_COLUMN_NAMES, 1, [Compile with support for alternate column names?])
					AC_MSG_RESULT([yes])
				else
					AC_MSG_RESULT([no])
				fi
			], AC_MSG_RESULT([no]))
	]
)


##
##	--enable-status enables the STATUS opcode to report server status
##
AC_DEFUN([AC_ENABLE_STATUS],
	[
		AC_MSG_CHECKING([whether to enable remote server status])
		AC_ARG_ENABLE(status,
			AC_HELP_STRING([--enable-status], [enable remote server status reports]),
			[
				if test "$enableval" = yes
				then
					AC_DEFINE(STATUS_ENABLED, 1, [Should remote server status be enabled?])
					AC_MSG_RESULT([yes])
				else
					AC_MSG_RESULT([no])
				fi
			], AC_MSG_RESULT([no]))
	]
)


##
## Find zlib compression library (@LIBZ@)
##
AC_DEFUN([AC_LIB_Z],
	[
		ac_zlib_dirs="/lib /usr/lib /usr/local/lib"
		AC_ARG_WITH(zlib,
			AC_HELP_STRING([--with-zlib=DIR], [look for the zlib compression library in DIR]),
			ac_zlib_dirs="$withval $ac_zlib_dirs")
		ac_zlib_found=no, ac_zlib_ok=no
		for dir in $ac_zlib_dirs
		do
			if test "$ac_zlib_found" != yes
			then
				AC_CHECK_FILE($dir/libz.a, ac_zlib_found=yes, ac_zlib_found=no)
				if test "$ac_zlib_found" = yes
				then
					AC_CHECK_LIB(z, deflate, ac_zlib_ok=yes, ac_zlib_ok=no)
					if test "$ac_zlib_ok" = yes
					then
						LIBZ="-L$dir -lz"
					fi
				fi
			fi
		done
		AC_SUBST(LIBZ)
	]
)


##
## Check to see if we're likely to need -lsocket
##
AC_DEFUN([AC_LIB_SOCKET],
	[
		AC_CHECK_LIB(socket, socket, ac_mydns_need_libsocket=yes, ac_mydns_need_libsocket=no)
		test "$ac_mydns_need_libsocket" = yes && LIBSOCKET="-lsocket"
		AC_SUBST(LIBSOCKET)
	]
)

##
## Check to see if we're likely to need -lnsl
##
AC_DEFUN([AC_LIB_NSL],
	[
		AC_CHECK_LIB(nsl, inet_addr, ac_mydns_need_libnsl=yes, ac_mydns_need_libnsl=no)
		test "$ac_mydns_need_libnsl" = yes && LIBNSL="-lnsl"
		AC_SUBST(LIBNSL)
	]
)


##
## Check to see if we're likely to need -lm
##
AC_DEFUN([AC_LIB_MATH],
	[
		AC_CHECK_LIB(m, floor, ac_mydns_need_libm=yes, ac_mydns_need_libm=no)
		test "$ac_mydns_need_libm" = yes && LIBM="-lm"
		AC_SUBST(LIBM)
	]
)


##
## Find MySQL client library (@LIBMYSQLCLIENT@)
##
AC_DEFUN([AC_LIB_MYSQLCLIENT],
	[
		libmysqlclient_dirs="/usr/local/mysql/lib /usr/local/lib/mysql /usr/local/lib /usr/lib/mysql /usr/lib /lib"
		AC_ARG_WITH(mysql-lib,
			AC_HELP_STRING([--with-mysql-lib=DIR], [look for the MySQL client library in DIR]),
			libmysqlclient_dirs="$withval $libmysqlclient_dirs")
		libmysqlclient_found=no, libmysqlclient_ok=no
		for libmysqlclient_dir in $libmysqlclient_dirs; do
			if test "$libmysqlclient_found" != yes; then
				AC_CHECK_FILE($libmysqlclient_dir/libmysqlclient.a, libmysqlclient_found=yes, libmysqlclient_found=no)
				if test "$libmysqlclient_found" != yes; then
					AC_CHECK_FILE($libmysqlclient_dir/libmysqlclient.so.10, libmysqlclient_found=yes, libmysqlclient_found=no)
				fi
				if test "$libmysqlclient_found" != yes; then
					AC_CHECK_FILE($libmysqlclient_dir/libmysqlclient.so.12, libmysqlclient_found=yes, libmysqlclient_found=no)
				fi
				if test "$libmysqlclient_found" = yes; then
					## libmysqlclient depends on libz
					if ! test -n "$LIBZ"; then
						AC_LIB_Z
					fi
					if ! test -n "$LIBZ"; then
						## No zlib
						AC_MSG_ERROR([

][  ###
][  ###  zlib compression library (libz.a) not found.
][  ###
][  ###  Please download and install the zlib compression
][  ###  library from the following URL:
][  ###
][  ###       http://www.gzip.org/zlib/
][  ###
][  ###  (Error detail might be available in `config.log')
][  ###
])
     				fi
					LIBMYSQLCLIENT="-L$libmysqlclient_dir -lmysqlclient"
					#LIBMYSQLCLIENT="$libmysqlclient_dir/libmysqlclient.a"
					libmysqlclient_found=yes
				fi
			fi
		done
		AC_SUBST(LIBMYSQLCLIENT)
	]
)

##
##	Find location of MySQL header files (@MYSQL_INCLUDE@)
##
AC_DEFUN([AC_HEADER_MYSQL],
	[
		ac_mydns_header_dirs="/usr/include /usr/include/mysql /usr/local/include \
									 /usr/local/include/mysql /usr/local/mysql/include"
		ac_mydns_header_found=no, ac_mydns_header_ok=no
		AC_ARG_WITH(mysql-include,
			AC_HELP_STRING([--with-mysql-include=DIR],
								[look for MySQL include files in DIR]),
			ac_mydns_header_dirs="$withval $ac_mydns_header_dirs")
		for dir in $ac_mydns_header_dirs
		do
			if test "$ac_mydns_header_found" != yes
			then
				AC_CHECK_FILE($dir/mysql.h, ac_mydns_header_found=yes, ac_mydns_header_found=no)
				test "$ac_mydns_header_found" = yes && MYSQL_INCLUDE="-I$dir"
			fi
		done
		AC_SUBST(MYSQL_INCLUDE)
	]
)


##
## Check for MySQL support
##
AC_DEFUN([AC_CHECK_MYSQL],
	[
		AC_MSG_CHECKING([whether to support MySQL])
		AC_ARG_WITH(mysql,
			AC_HELP_STRING([--without-mysql], [configure without MySQL support]),
			[
				if test "$withval" = no
				then
					AC_MSG_RESULT([no])
				else
					AC_MSG_RESULT([yes])
					AC_LIB_MYSQLCLIENT
					AC_HEADER_MYSQL
				fi
			], [
				AC_MSG_RESULT([yes])
				AC_LIB_MYSQLCLIENT
				AC_HEADER_MYSQL
			]
		)
	]
)


##
## Find PostgresSQL client library (@LIBPQ@)
##
AC_DEFUN([AC_LIB_PQ],
	[
		ac_mydns_lib_dirs="/usr/local/pgsql/lib /lib /usr/lib /usr/local/lib"
		AC_ARG_WITH(pgsql-lib,
			AC_HELP_STRING([--with-pgsql-lib=DIR],
								[look for the PostgreSQL client library in DIR]),
			ac_mydns_lib_dirs="$withval $ac_mydns_lib_dirs")
		ac_mydns_lib_found=no, ac_mydns_lib_ok=no
		ac_mydns_libs="$LDFLAGS"
		for dir in $ac_mydns_lib_dirs
		do
			if test "$ac_mydns_lib_found" != yes
			then
				AC_CHECK_FILE($dir/libpq.a, ac_mydns_lib_found=yes, ac_mydns_lib_found=no)
				if test "$ac_mydns_lib_found" = "yes"; then
					LIBPQ="-L$dir -lpq"

				fi
			fi
		done
		AC_SUBST(LIBPQ)
	]
)


##
##	Find location of PostgreSQL header files (@PQ_INCLUDE@)
##
AC_DEFUN([AC_HEADER_PQ],
	[
		ac_mydns_header_dirs="/usr/local/pgsql/include /usr/include/postgresql \
									 /usr/include /usr/local/include"
		ac_mydns_header_found=no, ac_mydns_header_ok=no
		AC_ARG_WITH(pgsql-include,
			AC_HELP_STRING([--with-pgsql-include=DIR],
								[look for PostgreSQL include files in DIR]),
			ac_mydns_header_dirs="$withval $ac_mydns_header_dirs")
		for dir in $ac_mydns_header_dirs
		do
			if test "$ac_mydns_header_found" != yes
			then
				AC_CHECK_FILE($dir/libpq-fe.h, ac_mydns_header_found=yes, ac_mydns_header_found=no)
				if test "$ac_mydns_header_found" = yes
				then
					PQ_INCLUDE="-I$dir"

					AC_MSG_CHECKING([for PostgreSQL version number])
					PG_VERSION=`grep "PG_VERSION " /usr/local/pgsql/include/pg_config.h | cut -f3 -d' '`
					AC_MSG_RESULT([$PG_VERSION])
					AC_DEFINE_UNQUOTED(PGSQL_VERSION, [$PG_VERSION], [PostgreSQL version])

					AC_CHECK_FILE($dir/pg_config.h, AC_DEFINE(HAVE_PGCONFIG, 1, [Does the system have pg_config.h?]))
				fi
			fi
		done
		AC_SUBST(PQ_INCLUDE)
	]
)


##
## Check for PostgreSQL support
##
AC_DEFUN([AC_CHECK_PGSQL],
	[
		AC_MSG_CHECKING([whether to support PostgreSQL])
		AC_ARG_WITH(pgsql,
			AC_HELP_STRING([--without-pgsql], [configure without PostgreSQL support]),
			[
				if test "$withval" = no
				then
					AC_MSG_RESULT([no])
				else
					AC_MSG_RESULT([yes])
					AC_LIB_PQ
					AC_HEADER_PQ
				fi
			], [
				AC_MSG_RESULT([yes])
				AC_LIB_PQ
				AC_HEADER_PQ
			]
		)
	]
)


##
## Choose between MySQL and PostgreSQL
##
AC_DEFUN([AC_CHOOSE_DB],
	[
		AC_MSG_CHECKING([for MySQL support])
		if test -n "$LIBMYSQLCLIENT" -a -n "$MYSQL_INCLUDE"; then
			AC_MSG_RESULT([yes])
			mysql_found=yes
		else
			AC_MSG_RESULT([no])
			mysql_found=no
		fi

		AC_MSG_CHECKING([for PostgreSQL support])
		if test -n "$LIBPQ" -a -n "$PQ_INCLUDE"; then
			AC_MSG_RESULT([yes])
			pgsql_found=yes
		else
			AC_MSG_RESULT([no])
			pgsql_found=no
		fi

		AC_MSG_CHECKING([which database to use])

		## Defaults for MySQL
		USE_DB_NAME="MySQL"
		LIBSQL="$LIBMYSQLCLIENT $LIBZ"
		SQLINCLUDE="$MYSQL_INCLUDE"

		if test "$mysql_found" = yes -a "$pgsql_found" = yes; then
			AC_DEFINE([USE_MYSQL], 1, [Use MySQL database?])
			AC_MSG_RESULT([MySQL])
			AC_MSG_NOTICE([

][  ###
][  ###  Both MySQL and PostgreSQL found!
][  ###
][  ###  MySQL used by default.  If you want to use
][  ###  PostgreSQL instead, rerun this script with:
][  ###
][  ###       --without-mysql
][  ###
])
		else
			if test "$mysql_found" = yes; then
				AC_DEFINE([USE_MYSQL], 1, [Use MySQL database?])
				AC_MSG_RESULT([MySQL])
			elif test "$pgsql_found" = yes; then
				AC_DEFINE([USE_PGSQL], 1, [Use PostgreSQL database?])
				AC_MSG_RESULT([PostgreSQL])
				USE_DB_NAME="PostgreSQL"
				LIBSQL="$LIBPQ"
				SQLINCLUDE="$PQ_INCLUDE"
			else
				AC_MSG_RESULT([none])
				AC_MSG_ERROR([No supported database found.  Either MySQL or PostgreSQL is required.])
			fi
		fi

		if test "$mysql_found" = yes; then
			AC_MSG_NOTICE([

][  ###
][  ###  If you have problems compiling with MySQL, specifically
][  ###  a "libz" linker error, please read README.mysql for
][  ###  more information.
][  ###
])

		fi
		AC_SUBST(USE_DB_NAME)
		AC_SUBST(LIBSQL)
		AC_SUBST(SQLINCLUDE)
	]
)


##
##  Set config file location
##
AC_DEFUN([AC_MYDNS_CONF],
	[
		AC_MSG_CHECKING([for default config file location])
		AC_ARG_WITH(confdir,
			AC_HELP_STRING([--with-confdir=DIR],
								[default config file path  (default is `/etc')]),
			ac_mydns_conf="$withval/mydns.conf", ac_mydns_conf="/etc/mydns.conf")
		MYDNS_CONF="$ac_mydns_conf"
		AC_SUBST(MYDNS_CONF)
		AC_DEFINE_UNQUOTED(MYDNS_CONF, ["$MYDNS_CONF"], [Default location of configuration file])
		AC_MSG_RESULT($MYDNS_CONF)
	]
)

##
## Set some extra variables to make changes easier
##
AC_DEFUN([AC_MYDNS_PKGINFO],
	[
		PACKAGE_DATE=`date +"%b %Y"`
		PACKAGE_RELEASE_DATE=`date +"%B %d, %Y"`

		AC_SUBST(PACKAGE_DATE)
		AC_DEFINE_UNQUOTED(PACKAGE_DATE, ["$PACKAGE_DATE"], [Package build date])

		AC_SUBST(PACKAGE_RELEASE_DATE)
		AC_DEFINE_UNQUOTED(PACKAGE_RELEASE_DATE, ["$PACKAGE_RELEASE_DATE"], [Exact package build date])

		AC_SUBST(PACKAGE_HOMEPAGE)
		AC_DEFINE_UNQUOTED(PACKAGE_HOMEPAGE, ["$PACKAGE_HOMEPAGE"], [This package's homepage])

		AC_SUBST(PACKAGE_COPYRIGHT)
		AC_DEFINE_UNQUOTED(PACKAGE_COPYRIGHT, ["$PACKAGE_COPYRIGHT"], [Copyright holder])

		COPYRIGHT_HOLDER=$PACKAGE_COPYRIGHT
		AC_SUBST(COPYRIGHT_HOLDER)

		AC_SUBST(PACKAGE_AUTHOR)
		AC_DEFINE_UNQUOTED(PACKAGE_AUTHOR, ["$PACKAGE_AUTHOR"], [This package's primary author])
	]
)


##
## Variables for use in makefiles
##
AC_DEFUN([AC_MYDNS_VARS],
	[
		## @WEBROOT@ def for builds on homepage
		WEBROOT="/www/htdocs/mydns"
		AC_SUBST(WEBROOT)

		## intl include dir
		INTLINCLUDE="-I\$(top_srcdir)/intl"
		AC_SUBST(INTLINCLUDE)

		## Include path and library for lib/ (libmydnsutil)
		UTILDIR="\$(top_srcdir)/lib"
		UTILINCLUDE="-I$UTILDIR"
		LIBUTIL="$UTILDIR/libmydnsutil.a"
		AC_SUBST(UTILDIR)
		AC_SUBST(UTILINCLUDE)
		AC_SUBST(LIBUTIL)

		## Include path and library for src/lib/ (libmydns)
		MYDNSDIR="\$(top_srcdir)/src/lib"
		MYDNSINCLUDE="-I$MYDNSDIR"
		LIBMYDNS="$MYDNSDIR/libmydns.a"
		AC_SUBST(MYDNSDIR)
		AC_SUBST(MYDNSINCLUDE)
		AC_SUBST(LIBMYDNS)

		## Documentation directory
		DOCDIR="\$(top_srcdir)/doc"
		AC_SUBST(DOCDIR)

		## "version.sed"
		SEDFILE="\$(top_srcdir)/version.sed"
		AC_SUBST(SEDFILE)
	]
)


## vi:set ts=3:
