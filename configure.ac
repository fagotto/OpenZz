dnl Process this file with autoconf to produce a configure script.
AC_INIT([ozz],[1.0.4-5],[matteo.fago@gmail.com])

AC_CONFIG_SRCDIR([src/sys.c])

dnl Added to activate automake
AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_PROG_CC

dnl Using 'AC_DISABLE_SHARED([])' is recomended to speed compile times
dnl  during development but that disables our dynamic lib loading
dnl  feature so I'm commenting it out.
dnl AC_DISABLE_SHARED([])

LT_INIT([dlopen])
AC_SUBST([LIBTOOL_DEPS])

AC_CONFIG_HEADERS([config.h])

AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_MKDIR_P

dnl Checks for libraries.
dnl Replaced second param:`main' with a function in -lreadline:
AC_CHECK_LIB(termcap, tputs)
AC_CHECK_LIB(readline, readline)

dnl JSB - include 'dl' lib for loading dynamic libraries.
dnl       This is used in sys.c - look for use of "dlopen":
AC_CHECK_LIB(dl, dlopen)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T

AC_CHECK_SIZEOF([char])
AC_CHECK_SIZEOF([short])
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([long])
AC_CHECK_SIZEOF([long long])
AC_CHECK_SIZEOF([char *])
AC_CHECK_SIZEOF([void *])

dnl Checks for library functions.
AC_CHECK_FUNCS(strdup strtol)

dnl JSB - need these to use libtool
LT_LIB_M
AC_SUBST([LIBM])

AC_CONFIG_FILES([Makefile \
          src/Makefile \
          doc/Makefile \
          doc/src/Makefile \
          testsuite/Makefile \
          ])

AC_OUTPUT
