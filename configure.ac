#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([curllog], [1.0], [allen@huarp.harvard.edu])
AC_CONFIG_SRCDIR([src/curl_obj.cc])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
#LT_INIT
AM_PROG_LIBTOOL
AC_SUBST([LIBTOOL_DEPS])
AC_SUBST([XML2CFLAGS])
AC_SUBST([XML2LIBS])
# AC_SUBST([CURLCFLAGS])
AC_SUBST([CURLLIBS])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_CHECK_PROG([XML2CONFIG],[xml2-config],[xml2-config],[none])
# on QNX6, xml2-config is not present, but extra args aren't required
if test $XML2CONFIG = xml2-config; then
  XML2CFLAGS=`xml2-config --cflags`
  XML2LIBS=`xml2-config --libs`
fi

AC_CHECK_PROG([CURLCONFIG],[curl-config],[curl-config],[none])
if test $CURLCONFIG = none; then AC_MSG_ERROR([Cannot locate curl-config]); fi
# CURLCFLAGS=`curl-config --cflags`
CURLLIBS=`curl-config --libs`

# Checks for libraries.
# xml2 and curl both provide config functions for finding their libraries,
# but I'm ignoring those because I think a simple -lxml2 -lcurl will do the job
# for now.
AC_CHECK_LIB([xml2], [htmlCreatePushParserCtxt],
  ,[AC_MSG_ERROR([Required library libxml2 not found])])
AC_CHECK_LIB([curl], [curl_global_init],
  ,[AC_MSG_ERROR([Required library libcurl not found])])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])
AC_CHECK_HEADER([curl/curl.h],[],[AC_MSG_ERROR([Required header curl/curl.h not found])])

# Check for xml2 headers
CPPFLAGS_SET=${CPPFLAGS+set}
SAVE_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS $XML2CFLAGS"
AC_CHECK_HEADER([libxml/HTMLparser.h],[],[AC_MSG_ERROR([Required header libxml/HTMLparser.h not found])])
if test "$CPPFLAGS_SET" = set; then
  CPPFLAGS=$SAVE_CPPFLAGS
else
  unset CPPFLAGS
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit clock_gettime mkdir select strdup strerror curl_multi_socket_action])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
