AC_PREREQ([2.69])
AC_INIT([ozz], [1.0.4-5], [matteo.fago@gmail.com])
AC_CONFIG_SRCDIR([src/sys.c])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
dnl The legacy callback ABI needs pre-C23 unprototyped function declarations.
dnl Keep this explicit even when new Autoconf selects GNU C23 automatically.
ZZ_CFLAGS=-std=gnu11
zz_saved_CFLAGS=$CFLAGS
CFLAGS="$ZZ_CFLAGS $CFLAGS"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [[
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L || __STDC_VERSION__ >= 202311L
#error OpenZz requires GNU C11 or C17 until its callback ABI is modernized
#endif
]])], [], [AC_MSG_ERROR([use GCC/Clang with GNU C11 or C17 support])])
CFLAGS=$zz_saved_CFLAGS
AC_SUBST([ZZ_CFLAGS])
LT_INIT([dlopen])
dnl Some sandboxed BSD/macOS hosts deny sysctl; never pass an empty limit to Libtool.
AS_IF([test -z "$max_cmd_len"], [
  AC_MSG_WARN([command length probe failed; using conservative 32768 byte limit])
  max_cmd_len=32768
  lt_cv_sys_max_cmd_len=$max_cmd_len
])

AC_CHECK_HEADERS([stdint.h dlfcn.h sys/times.h sys/resource.h], [],
  [AC_MSG_ERROR([OpenZz requires a POSIX platform with stdint.h and dlopen support])])
zz_saved_LIBS=$LIBS
AC_SEARCH_LIBS([dlopen], [dl], [], [AC_MSG_ERROR([dlopen is required])])
DL_LIBS=$ac_cv_search_dlopen
AS_IF([test "x$DL_LIBS" = "xnone required"], [DL_LIBS=])
LIBS=$zz_saved_LIBS
AC_SUBST([DL_LIBS])

AC_ARG_WITH([readline], AS_HELP_STRING([--without-readline],
  [use the standard input fallback instead of Readline]), [], [with_readline=auto])
AS_CASE([$with_readline], [yes|no|auto], [], [AC_MSG_ERROR([--with-readline accepts yes or no])])
READLINE_LIBS=
AS_IF([test "x$with_readline" != xno], [
  AC_CHECK_HEADERS([readline/readline.h readline/history.h])
  AS_IF([test "x$ac_cv_header_readline_readline_h" = xyes && test "x$ac_cv_header_readline_history_h" = xyes], [
    AC_SEARCH_LIBS([tputs], [tinfo ncurses termcap])
    AC_CHECK_LIB([readline], [readline], [
      READLINE_LIBS="-lreadline $LIBS"
      AC_DEFINE([HAVE_READLINE], [1], [Define when Readline is available])
    ])
  ])
  AS_IF([test "x$with_readline" = xyes && test -z "$READLINE_LIBS"],
    [AC_MSG_ERROR([Readline requested but headers or library were not found])])
])
LIBS=$zz_saved_LIBS
AC_SUBST([READLINE_LIBS])
LT_LIB_M
AC_SUBST([LIBM])
AC_CHECK_FUNCS([strdup strtol])
AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile doc/src/Makefile testsuite/Makefile])
AC_OUTPUT
