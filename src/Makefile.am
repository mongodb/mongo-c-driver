pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(top_builddir)/src/libmongoc-1.0.pc
if ENABLE_SSL
pkgconfig_DATA += $(top_builddir)/src/libmongoc-ssl-1.0.pc
endif

noinst_LTLIBRARIES += libmongoc-priv.la
lib_LTLIBRARIES += libmongoc-1.0.la

libmongoc_priv_la_CPPFLAGS =

if OS_SOLARIS
libmongoc_priv_la_CPPFLAGS += -D_REENTRANT
endif

libmongoc_priv_la_CPPFLAGS += \
	-DMONGOC_COMPILATION \
	$(LIBC_FEATURES) \
	$(OPTIMIZE_CFLAGS) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/mongoc \
	-I$(top_builddir)/src/mongoc \
	$(BSON_CFLAGS) \
	$(PTHREAD_CFLAGS) \
	$(SSL_CFLAGS) \
	$(SASL_CFLAGS)

if !ENABLE_DEBUG
libmongoc_priv_la_CPPFLAGS += \
	-DBSON_DISABLE_ASSERT \
	-DBSON_DISABLE_CHECKS
endif

libmongoc_priv_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS)

libmongoc_priv_la_LIBADD = \
	$(BSON_LIBS) \
	$(PTHREAD_LIBS) \
	$(SHM_LIB) \
	$(SSL_LIBS) \
	$(SASL_LIBS)

if OS_WIN32
libmongoc_priv_la_LIBADD += -lws2_32
endif


libmongoc_1_0_la_SOURCES = 
libmongoc_1_0_la_LIBADD = libmongoc-priv.la
libmongoc_1_0_la_LDFLAGS = \
	$(OPTIMIZE_CFLAGS) \
	$(MONGOC_LT_LDFLAGS) \
	-no-undefined \
	-export-symbols-regex "^mongoc_" \
	-rpath $(libdir)

include src/bindings/Makefile.am
include src/mongoc/Makefile.am
include src/tools/Makefile.am

EXTRA_DIST += \
	src/libmongoc.symbols \
	src/libmongoc-1.0.pc.in \
	src/libmongoc-ssl-1.0.pc.in
