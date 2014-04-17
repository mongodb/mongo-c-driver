DOC_EXAMPLES = examples/example-client.c

EXAMPLE_LDADD = libmongoc-1.0.la
if EXPLICIT_LIBS
EXAMPLE_LDADD += $(BSON_LIBS)
endif
EXAMPLE_CFLAGS = -I$(top_srcdir)/src/mongoc $(BSON_CFLAGS)

noinst_PROGRAMS += example-gridfs
example_gridfs_SOURCES = examples/example-gridfs.c
example_gridfs_CFLAGS = $(EXAMPLE_CFLAGS)
example_gridfs_LDADD = $(EXAMPLE_LDADD)

noinst_PROGRAMS += mongoc-dump
mongoc_dump_SOURCES = examples/mongoc-dump.c
mongoc_dump_CFLAGS = $(EXAMPLE_CFLAGS)
mongoc_dump_LDADD = $(EXAMPLE_LDADD)

noinst_PROGRAMS += filter-bsondump
filter_bsondump_SOURCES = examples/filter-bsondump.c
filter_bsondump_CFLAGS = $(EXAMPLE_CFLAGS)
filter_bsondump_LDADD = $(EXAMPLE_LDADD)

noinst_PROGRAMS += example-client
example_client_SOURCES = examples/example-client.c
example_client_CFLAGS = $(EXAMPLE_CFLAGS)
example_client_LDADD = $(EXAMPLE_LDADD)

noinst_PROGRAMS += mongoc-ping
mongoc_ping_SOURCES = examples/mongoc-ping.c
mongoc_ping_CFLAGS = $(EXAMPLE_CFLAGS)
mongoc_ping_LDADD = $(EXAMPLE_LDADD)

noinst_PROGRAMS += mongoc-rpc-validate
mongoc_rpc_validate_SOURCES = examples/mongoc-rpc-validate.c
mongoc_rpc_validate_CFLAGS = -DMONGOC_COMPILATION $(EXAMPLE_CFLAGS)
mongoc_rpc_validate_LDADD = libmongoc-priv.la

noinst_PROGRAMS += mongoc-tail
mongoc_tail_SOURCES = examples/mongoc-tail.c
mongoc_tail_CFLAGS = $(EXAMPLE_CFLAGS)
mongoc_tail_LDADD = $(EXAMPLE_LDADD)

noinst_PROGRAMS += find-and-modify
find_and_modify_SOURCES = examples/find-and-modify.c
find_and_modify_CFLAGS = $(EXAMPLE_CFLAGS)
find_and_modify_LDADD = $(EXAMPLE_LDADD)
