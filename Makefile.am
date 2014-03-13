EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =

bin_PROGRAMS =
lib_LTLIBRARIES =
noinst_LTLIBRARIES =
noinst_PROGRAMS =

if WITH_LIBBSON
SUBDIRS = src/libbson
BSON_CFLAGS = \
	-I$(top_srcdir)/src/libbson/src/bson \
	-I$(top_builddir)/src/libbson/src/bson
BSON_LIBS = $(top_builddir)/src/libbson/libbson-1.0.la
endif

AM_CTAGSFLAGS = --fields=+l --languages=c

include doc/Makefile.am
include examples/Makefile.am
include src/Makefile.am
include tests/Makefile.am

# for EXTRA_DIST
include build/cmake/Makefile.am

ACLOCAL_AMFLAGS = -I build/autotools/m4 ${ACLOCAL_FLAGS}
DISTCHECK_CONFIGURE_FLAGS = --enable-silent-rules

mongocdocdir = ${docdir}
mongocdoc_DATA = \
	COPYING \
	NEWS \
	README.rst
EXTRA_DIST += $(mongocdoc_DATA)

EXTRA_DIST += \
	CMakeLists.txt \
	CONTRIBUTING.md

dist-hook:
	@if test -d "$(srcdir)/.git"; then                              \
          (cd "$(srcdir)" &&                                            \
           $(top_srcdir)/missing --run git log --stat ) > ChangeLog.tmp \
           && mv -f ChangeLog.tmp $(top_distdir)/ChangeLog              \
           || (rm -f ChangeLog.tmp;                                     \
               echo Failed to generate ChangeLog >&2);                  \
	else                                                            \
	  echo A git checkout is required to generate a ChangeLog >&2;  \
	fi

uninstall-local:
	-rm -r $(mongocdocdir)
