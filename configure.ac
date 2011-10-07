#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([curllog], [1.0], [allen@huarp.harvard.edu])
AC_CONFIG_SRCDIR([src/curltest.cc])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE
LT_INIT
AC_SUBST([LIBTOOL_DEPS])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([xml2], [htmlCreatePushParserCtxt],
  ,[AC_MSG_ERROR([Required library libxml2 not found])])
AC_CHECK_LIB([curl], [curl_global_init],
  ,[AC_MSG_ERROR([Required library libcurl not found])])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])
AC_CHECK_HEADER([curl/curl.h],[],[AC_MSG_ERROR([Required header curl/curl.h not found])])
AC_CHECK_HEADER([libxml/HTMLparser.h],[],[AC_MSG_ERROR([Required header libxml/HTMLparser.h not found])])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit clock_gettime mkdir select strdup strerror])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
