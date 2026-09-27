AM_CFLAGS = $(ZZ_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)
TESTS = boolean.zz \
 deref_param.zz \
 double.zz \
 foreach.zz \
 integer.zz \
 listmerge.zz \
 listsplit.zz \
 looptest_do.zz \
 looptest_while.zz \
 numerictypes.zz \
 popvoidrule.zz \
 setcase.zz \
 setparseeol.zz \
 setparseeol2.zz \
 tagdtor.zz \
 utilities.zz \
 when_del_scope.zz
EXTRA_DIST = $(TESTS) zz_test.sh list_in_cond.zz.SKIPPED
TEST_EXTENSIONS = .zz .sh
SH_LOG_COMPILER = $(SHELL)
ZZ_LOG_COMPILER = $(SHELL) $(srcdir)/zz_test.sh
AM_TESTS_ENVIRONMENT = ZZ='$(abs_top_builddir)/src/ozz'; export ZZ; TEST_BUILDDIR='$(abs_builddir)'; export TEST_BUILDDIR;
check_LTLIBRARIES = libtagdtor.la
libtagdtor_la_SOURCES = tagdtor.c
libtagdtor_la_LIBADD = ../src/libozz.la
libtagdtor_la_LDFLAGS = -module -avoid-version -rpath $(abs_builddir)
.PHONY: zztest
zztest: check

check_PROGRAMS = robustness
robustness_SOURCES = robustness.c
robustness_LDADD = ../src/libozz.la
TESTS += robustness cli-smoke.sh
CLEANFILES = noname.err

check_PROGRAMS += token-input
token_input_SOURCES = token-input.c
token_input_LDADD = ../src/libozz.la
TESTS += token-input
