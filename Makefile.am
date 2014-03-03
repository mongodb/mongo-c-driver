EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =

bin_PROGRAMS =
lib_LTLIBRARIES =
noinst_LTLIBRARIES =
noinst_PROGRAMS =

if WITH_LIBBSON
SUBDIRS = src/libbson
endif

AM_CPPFLAGS = \
	-DMONGOC_COMPILATION \
	-D_BSD_SOURCE \
	-D_GNUC_SOURCE \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/mongoc \
	-I$(top_builddir)/src/mongoc

LIBS = $(SHM_LIB)

if WITH_LIBBSON
AM_CPPFLAGS += -I$(top_srcdir)/src/libbson/src/bson
LIBS += libbson-1.0.la
else
AM_CPPFLAGS += $(BSON_CFLAGS)
LIBS += $(BSON_LIBS)
endif

AM_CTAGSFLAGS = --fields=+l --languages=c

include doc/Makefile.am
include examples/Makefile.am
include src/Makefile.am
include tests/Makefile.am

ACLOCAL_AMFLAGS = -I build/autotools/m4 ${ACLOCAL_FLAGS}
DISTCHECK_CONFIGURE_FLAGS = --enable-silent-rules

mongocdocdir = ${docdir}
mongocdoc_DATA = \
	COPYING \
	NEWS \
	README.rst
EXTRA_DIST += $(mongocdoc_DATA)

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
