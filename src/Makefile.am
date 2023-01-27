## INCLUDES
## LDFLAGS
## LDADD
## EXTRADIST
## SUBDIRS
## check_PROGRAMS
## TESTS

lib_LTLIBRARIES = libozz.la libozzi.la

libozz_la_SOURCES=err.c source.c zlex.c list.c rule.c scope.c table.c dumpnet.c \
	      parse.c param.c action.c lazy.c zkernel.c zsys.c kernel.c \
              sys.c interface.c defopen.c avl.c printz.c util.c \
	      avl.h kernel.h mem.h rule.h sys.h zlex.h \
	      err.h list.h param.h printz.h source.h trace.h table.h
libozz_la_LIBADD = $(LIBM)

libozz_la_LDFLAGS = -version-info 1:4:1 -export-dynamic

## ZZ Interactive library
libozzi_la_SOURCES= zzi.c

libozzi_la_LDFLAGS = -version-info 1:4:1 -export-dynamic



## headers to be installed
openzzincludedir = $(includedir)/ozz
openzzinclude_HEADERS= zz.h zzbind.h


bin_PROGRAMS=ozz
ozz_SOURCES = zz.c
ozz_LDADD = libozzi.la libozz.la


# zz_LDFLAGS =
# zz_DEPENDENCIES=libzz
