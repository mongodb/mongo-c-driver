noinst_PROGRAMS += test-load
noinst_PROGRAMS += test-secondary
noinst_PROGRAMS += test-replica-set
noinst_PROGRAMS += test-sharded-cluster
noinst_PROGRAMS += test-libmongoc
noinst_PROGRAMS += echo-server
if ENABLE_SSL
noinst_PROGRAMS += test-replica-set-ssl
endif


TEST_PROGS = test-libmongoc


if OS_LINUX
TEST_PROGS += tests/abicheck.sh
endif


TEST_CFLAGS = \
	-DMONGOC_COMPILATION \
	$(BSON_CFLAGS) \
	-I$(top_srcdir)/src/mongoc \
	-I$(top_builddir)/src/mongoc

TEST_LIBS = libmongoc-priv.la $(BSON_LIBS) $(SHM_LIB)
if OS_WIN32
TEST_LIBS += -lshlwapi
endif

test_load_SOURCES = \
	tests/test-load.c \
	tests/mongoc-tests.c
test_load_CFLAGS = $(TEST_CFLAGS)
test_load_LDADD = $(TEST_LIBS)


echo_server_SOURCES = tests/echo-server.c
echo_server_CFLAGS = $(TEST_CFLAGS)
echo_server_LDADD = $(TEST_LIBS)


test_secondary_SOURCES = \
	tests/test-secondary.c \
	tests/mongoc-tests.c
test_secondary_CFLAGS = $(TEST_CFLAGS)
test_secondary_LDADD = $(TEST_LIBS)

test_replica_set_SOURCES = \
	tests/test-replica-set.c \
	tests/ha-test.c \
	tests/ha-test.h \
	tests/mongoc-tests.c
test_replica_set_CFLAGS = $(TEST_CFLAGS)
test_replica_set_LDADD = $(TEST_LIBS)


test_replica_set_ssl_SOURCES = \
	tests/test-replica-set-ssl.c \
	tests/ha-test.c \
	tests/ha-test.h \
	tests/mongoc-tests.c
test_replica_set_ssl_CFLAGS = $(TEST_CFLAGS)
test_replica_set_ssl_LDADD = $(TEST_LIBS)


test_libmongoc_SOURCES = \
	tests/mock-server.c \
	tests/mock-server.h \
	tests/test-libmongoc.c \
	tests/test-mongoc-array.c \
	tests/test-mongoc-buffer.c \
	tests/test-mongoc-client.c \
	tests/test-mongoc-client-pool.c \
	tests/test-mongoc-collection.c \
	tests/test-mongoc-cursor.c \
	tests/test-mongoc-database.c \
	tests/test-mongoc-gridfs.c \
	tests/test-mongoc-gridfs-file-page.c \
	tests/test-mongoc-list.c \
	tests/test-mongoc-matcher.c \
	tests/test-mongoc-queue.c \
	tests/test-mongoc-read-prefs.c \
	tests/test-mongoc-rpc.c \
	tests/test-mongoc-stream.c \
	tests/test-mongoc-uri.c \
	tests/test-mongoc-write-concern.c \
	tests/test-libmongoc.h \
	tests/TestSuite.c \
	tests/TestSuite.h
if ENABLE_SSL
test_libmongoc_SOURCES += \
	tests/test-x509.c \
	tests/test-mongoc-stream-tls.c \
	tests/ssl-test.c \
	tests/ssl-test.h
endif
test_libmongoc_CFLAGS = $(TEST_CFLAGS)
test_libmongoc_LDADD = $(TEST_LIBS)


test_sharded_cluster_SOURCES = \
	tests/test-sharded-cluster.c \
	tests/ha-test.c \
	tests/ha-test.h \
	tests/mongoc-tests.c
test_sharded_cluster_CFLAGS = $(TEST_CFLAGS)
test_sharded_cluster_LDADD = $(TEST_LIBS)

test_certs: $(top_builddir)/tests/trust_dir/done

tests/trust_dir/done: $(top_srcdir)/tests/make_ca.pl $(top_srcdir)/tests/trust_dir.cnf
	$< $(top_srcdir)/tests/trust_dir $(top_srcdir)/tests/trust_dir.cnf
	touch $(top_srcdir)/tests/trust_dir/done

if ENABLE_SSL
test: $(TEST_PROGS) test_certs
else
test: $(TEST_PROGS)
endif
	@ for TEST_PROG in $(TEST_PROGS) ; do \
		./$$TEST_PROG -F test.log; \
	done

.PHONY: test_certs

EXTRA_DIST += \
	tests/binary/delete1.dat \
	tests/binary/get_more1.dat \
	tests/binary/gridfs.dat \
	tests/binary/insert1.dat \
	tests/binary/kill_cursors1.dat \
	tests/binary/msg1.dat \
	tests/binary/query1.dat \
	tests/binary/query2.dat \
	tests/binary/reply1.dat \
	tests/binary/reply2.dat \
	tests/binary/update1.dat \
	tests/mock-server.c \
	tests/mock-server.h \
	tests/mongoc-tests.c \
	tests/mongoc-tests.h
