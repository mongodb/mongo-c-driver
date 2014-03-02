headerdir = $(prefix)/include/libmongoc-@BSON_API_VERSION@
header_DATA = $(INST_H_FILES)

INST_H_FILES = \
	src/mongoc/mongoc.h \
	src/mongoc/mongoc-client.h \
	src/mongoc/mongoc-client-pool.h \
	src/mongoc/mongoc-collection.h \
	src/mongoc/mongoc-compat.h \
	src/mongoc/mongoc-compat-socket-unix.h \
	src/mongoc/mongoc-config.h \
	src/mongoc/mongoc-cursor.h \
	src/mongoc/mongoc-database.h \
	src/mongoc/mongoc-error.h \
	src/mongoc/mongoc-flags.h \
	src/mongoc/mongoc-gridfs.h \
	src/mongoc/mongoc-gridfs-file.h \
	src/mongoc/mongoc-gridfs-file-page.h \
	src/mongoc/mongoc-gridfs-file-list.h \
	src/mongoc/mongoc-host-list.h \
	src/mongoc/mongoc-init.h \
	src/mongoc/mongoc-index.h \
	src/mongoc/mongoc-log.h \
	src/mongoc/mongoc-matcher.h \
	src/mongoc/mongoc-opcode.h \
	src/mongoc/mongoc-read-prefs.h \
	src/mongoc/mongoc-stream.h \
	src/mongoc/mongoc-stream-buffered.h \
	src/mongoc/mongoc-stream-gridfs.h \
	src/mongoc/mongoc-stream-unix.h \
	src/mongoc/mongoc-thread.h \
	src/mongoc/mongoc-uri.h \
	src/mongoc/mongoc-version.h \
	src/mongoc/mongoc-write-concern.h

if OS_WIN32
	INST_H_FILES += src/mongoc/mongoc-compat-socket-win32.h
endif

if ENABLE_SSL
INST_H_FILES += \
	mongoc/mongoc-stream-tls.h \
	mongoc/mongoc-ssl.h
endif

libmongoc_priv_la_SOURCES = \
	$(INST_H_FILES) \
	$(NOINST_H_FILES) \
	src/mongoc/mongoc-array.c \
	src/mongoc/mongoc-buffer.c \
	src/mongoc/mongoc-client.c \
	src/mongoc/mongoc-client-pool.c \
	src/mongoc/mongoc-cluster.c \
	src/mongoc/mongoc-collection.c \
	src/mongoc/mongoc-compat-unix.c \
	src/mongoc/mongoc-compat-socket-unix.c \
	src/mongoc/mongoc-counters.c \
	src/mongoc/mongoc-cursor.c \
	src/mongoc/mongoc-cursor-array.c \
	src/mongoc/mongoc-cursor-cursorid.c \
	src/mongoc/mongoc-database.c \
	src/mongoc/mongoc-init.c \
	src/mongoc/mongoc-gridfs.c \
	src/mongoc/mongoc-gridfs-file.c \
	src/mongoc/mongoc-gridfs-file-page.c \
	src/mongoc/mongoc-gridfs-file-list.c \
	src/mongoc/mongoc-index.c \
	src/mongoc/mongoc-list.c \
	src/mongoc/mongoc-log.c \
	src/mongoc/mongoc-matcher-op.c \
	src/mongoc/mongoc-matcher.c \
	src/mongoc/mongoc-queue.c \
	src/mongoc/mongoc-read-prefs.c \
	src/mongoc/mongoc-rpc.c \
	src/mongoc/mongoc-stream.c \
	src/mongoc/mongoc-stream-buffered.c \
	src/mongoc/mongoc-stream-gridfs.c \
	src/mongoc/mongoc-stream-unix.c \
	src/mongoc/mongoc-uri.c \
	src/mongoc/mongoc-util.c \
	src/mongoc/mongoc-write-concern.c

if OS_WIN32
libmongoc_priv_la_SOURCES += \
	src/mongoc/mongoc-compat-win32.c \
	src/mongoc/mongoc-compat-socket-win32.c
endif

if ENABLE_SSL
libmongoc_priv_la_SOURCES += \
	mongoc/mongoc-stream-tls.c \
	mongoc/mongoc-ssl.c
endif

if ENABLE_SASL
libmongoc_priv_la_SOURCES += mongoc/mongoc-sasl.c
endif

EXTRA_DIST += \
	src/mongoc/op-delete.def \
	src/mongoc/op-get-more.def \
	src/mongoc/op-header.def \
	src/mongoc/op-insert.def \
	src/mongoc/op-kill-cursors.def \
	src/mongoc/op-msg.def \
	src/mongoc/op-query.def \
	src/mongoc/op-reply.def \
	src/mongoc/op-update.def \
	src/mongoc/mongoc-counters.defs
