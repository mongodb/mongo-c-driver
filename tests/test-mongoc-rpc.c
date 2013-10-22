#include <fcntl.h>
#include <mongoc.h>
#include <mongoc-array-private.h>
#include <mongoc-rpc-private.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mongoc-tests.h"


static bson_uint8_t *
get_test_file (const char *filename,
               size_t     *length)
{
   bson_uint32_t len;
   bson_uint8_t *buf;
   char real_filename[256];
   int fd;

   snprintf(real_filename, sizeof real_filename,
            "tests/binary/%s", filename);
   real_filename[sizeof real_filename - 1] = '\0';

   if (-1 == (fd = open(real_filename, O_RDONLY))) {
      fprintf(stderr, "Failed to open: %s\n", real_filename);
      abort();
   }

   len = 40960;
   buf = bson_malloc0(len);
   len = read(fd, buf, len);
   assert(len > 0);

   *length = len;
   return buf;
}


#define RPC(_name, _code) \
static inline bson_bool_t \
test_mongoc_rpc_##_name##_equal (const mongoc_rpc_##_name##_t *a, \
                                 const mongoc_rpc_##_name##_t *b) \
{ \
   _code \
}

#define INT32_FIELD(_name)             if (a->_name != b->_name) return FALSE;
#define INT64_FIELD(_name)             if (a->_name != b->_name) return FALSE;
#define CSTRING_FIELD(_name)           if (!!strcmp(a->_name, b->_name)) return FALSE;
#define BSON_FIELD(_name) \
   { \
      bson_uint32_t alen; \
      bson_uint32_t blen; \
      memcpy(&alen, a->_name, 4); \
      memcpy(&blen, b->_name, 4); \
      alen = BSON_UINT32_FROM_LE(alen); \
      blen = BSON_UINT32_FROM_LE(blen); \
      if (!!memcmp(a->_name, b->_name, alen)) { \
         return FALSE; \
      } \
   }
#define BSON_ARRAY_FIELD(_name)        if ((a->_name##_len != b->_name##_len) || !!memcmp(a->_name, b->_name, a->_name##_len)) return FALSE;
#define INT64_ARRAY_FIELD(_len, _name) if ((a->_len != b->_len) || !!memcmp(a->_name, b->_name, a->_len * 8)) return FALSE;
#define RAW_BUFFER_FIELD(_name)        if ((a->_name##_len != b->_name##_len) || !!memcmp(a->_name, b->_name, a->_name##_len)) return FALSE;
#define OPTIONAL(_check, _code)        if (!!a->_check != !!b->_check) { return FALSE; } _code


#include "op-reply.def"
#include "op-msg.def"
#include "op-update.def"
#include "op-insert.def"
#include "op-query.def"
#include "op-get-more.def"
#include "op-delete.def"
#include "op-kill-cursors.def"


#undef RPC
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef OPTIONAL
#undef RAW_BUFFER_FIELD


static inline bson_bool_t
test_mongoc_rpc_equal (const mongoc_rpc_t *a,
                       const mongoc_rpc_t *b)
{
   if (a->header.opcode != b->header.opcode) {
      return FALSE;
   }

   switch (a->header.opcode) {
   case MONGOC_OPCODE_REPLY:
      return test_mongoc_rpc_reply_equal(&a->reply, &b->reply);
   case MONGOC_OPCODE_MSG:
      return test_mongoc_rpc_msg_equal(&a->msg, &b->msg);
   case MONGOC_OPCODE_UPDATE:
      return test_mongoc_rpc_update_equal(&a->update, &b->update);
   case MONGOC_OPCODE_INSERT:
      return test_mongoc_rpc_insert_equal(&a->insert, &b->insert);
   case MONGOC_OPCODE_QUERY:
      return test_mongoc_rpc_query_equal(&a->query, &b->query);
   case MONGOC_OPCODE_GET_MORE:
      return test_mongoc_rpc_get_more_equal(&a->get_more, &b->get_more);
   case MONGOC_OPCODE_DELETE:
      return test_mongoc_rpc_delete_equal(&a->delete, &b->delete);
   case MONGOC_OPCODE_KILL_CURSORS:
      return test_mongoc_rpc_kill_cursors_equal(&a->kill_cursors, &b->kill_cursors);
   default:
      return FALSE;
   }
}


/*
 * This function expects that @rpc is in HOST ENDIAN format.
 */
static void
assert_rpc_equal (const char   *filename,
                  mongoc_rpc_t *rpc)
{
   mongoc_array_t ar;
   bson_uint8_t *data;
   struct iovec *iov;
   size_t length;
   off_t off = 0;
   int r;
   int i;

   data = get_test_file(filename, &length);
   mongoc_array_init(&ar, sizeof(struct iovec));

   /*
    * Gather our RPC into a series of iovec that can be compared
    * to the buffer from the RCP snapshot file.
    */
   mongoc_rpc_gather(rpc, &ar);

#if 0
   printf("Before swabbing\n");
   printf("=========================\n");
   mongoc_rpc_printf(rpc);
#endif

   mongoc_rpc_swab_to_le(rpc);

#if 0
   printf("After swabbing\n");
   printf("=========================\n");
   mongoc_rpc_printf(rpc);
#endif

   for (i = 0; i < ar.len; i++) {
      iov = &mongoc_array_index(&ar, struct iovec, i);
      assert(iov->iov_len <= (length - off));
      r = memcmp(&data[off], iov->iov_base, iov->iov_len);
      if (r) {
         printf("\nError iovec: %u\n", i);
      }
      assert(r == 0);
      off += iov->iov_len;
   }

   mongoc_array_destroy(&ar);
   bson_free(data);
}


static void
test_mongoc_rpc_delete_gather (void)
{
   mongoc_rpc_t rpc;
   bson_t sel;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   bson_init(&sel);

   rpc.delete.msg_len = 0;
   rpc.delete.request_id = 1234;
   rpc.delete.response_to = -1;
   rpc.delete.opcode = MONGOC_OPCODE_DELETE;
   rpc.delete.zero = 0;
   rpc.delete.collection = "test.test";
   rpc.delete.flags = MONGOC_DELETE_SINGLE_REMOVE;
   rpc.delete.selector = bson_get_data(&sel);

   assert_rpc_equal("delete1.dat", &rpc);
}


static void
test_mongoc_rpc_delete_scatter (void)
{
   bson_uint8_t *data;
   mongoc_rpc_t rpc;
   bson_bool_t r;
   bson_t sel;
   size_t length;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   bson_init(&sel);

   data = get_test_file("delete1.dat", &length);
   r = mongoc_rpc_scatter(&rpc, data, length);
   assert(r);
   mongoc_rpc_swab_from_le(&rpc);

   assert_cmpint(rpc.delete.msg_len, ==, 39);
   assert_cmpint(rpc.delete.request_id, ==, 1234);
   assert_cmpint(rpc.delete.response_to, ==, -1);
   assert_cmpint(rpc.delete.opcode, ==, MONGOC_OPCODE_DELETE);
   assert_cmpint(rpc.delete.zero, ==, 0);
   assert(!strcmp("test.test", rpc.delete.collection));
   assert_cmpint(rpc.delete.flags, ==, MONGOC_DELETE_SINGLE_REMOVE);
   assert(!memcmp(rpc.delete.selector, bson_get_data(&sel), sel.len));

   assert_rpc_equal("delete1.dat", &rpc);
   bson_free(data);
}


static void
test_mongoc_rpc_get_more_gather (void)
{
   mongoc_rpc_t rpc;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   rpc.get_more.msg_len = 0;
   rpc.get_more.request_id = 1234;
   rpc.get_more.response_to = -1;
   rpc.get_more.opcode = MONGOC_OPCODE_GET_MORE;
   rpc.get_more.zero = 0;
   rpc.get_more.collection = "test.test";
   rpc.get_more.n_return = 5;
   rpc.get_more.cursor_id = 12345678L;

   assert_rpc_equal("get_more1.dat", &rpc);
}


static void
test_mongoc_rpc_get_more_scatter (void)
{
   bson_uint8_t *data;
   mongoc_rpc_t rpc;
   bson_bool_t r;
   size_t length;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   data = get_test_file("get_more1.dat", &length);
   r = mongoc_rpc_scatter(&rpc, data, length);
   assert(r);
   mongoc_rpc_swab_from_le(&rpc);

   assert(rpc.get_more.msg_len == 42);
   assert(rpc.get_more.request_id == 1234);
   assert(rpc.get_more.response_to == -1);
   assert(rpc.get_more.opcode == MONGOC_OPCODE_GET_MORE);
   assert(rpc.get_more.zero == 0);
   assert(!strcmp("test.test", rpc.get_more.collection));
   assert(rpc.get_more.n_return == 5);
   assert(rpc.get_more.cursor_id == 12345678);

   assert_rpc_equal("get_more1.dat", &rpc);
   bson_free(data);
}


static void
test_mongoc_rpc_insert_gather (void)
{
   bson_writer_t *writer;
   mongoc_rpc_t rpc;
   bson_uint8_t *buf = NULL;
   size_t len = 0;
   bson_t *b;
   int i;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   writer = bson_writer_new(&buf, &len, 0, bson_realloc);
   for (i = 0; i < 20; i++) {
      bson_writer_begin(writer, &b);
      bson_writer_end(writer);
   }

   rpc.insert.msg_len = 0;
   rpc.insert.request_id = 1234;
   rpc.insert.response_to = -1;
   rpc.insert.opcode = MONGOC_OPCODE_INSERT;
   rpc.insert.flags = MONGOC_INSERT_CONTINUE_ON_ERROR;
   rpc.insert.collection = "test.test";
   rpc.insert.documents = buf;
   rpc.insert.documents_len = bson_writer_get_length(writer);

   assert_rpc_equal("insert1.dat", &rpc);
   bson_writer_destroy(writer);
   bson_free(buf);
}


static void
test_mongoc_rpc_insert_scatter (void)
{
   bson_reader_t *reader;
   bson_uint8_t *data;
   const bson_t *b;
   mongoc_rpc_t rpc;
   bson_bool_t r;
   bson_bool_t eof = FALSE;
   size_t length;
   bson_t empty;
   int count = 0;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   bson_init(&empty);

   data = get_test_file("insert1.dat", &length);
   r = mongoc_rpc_scatter(&rpc, data, length);
   assert(r);

   assert_cmpint(rpc.insert.msg_len, ==, 130);
   assert_cmpint(rpc.insert.request_id, ==, 1234);
   assert_cmpint(rpc.insert.response_to, ==, (bson_uint32_t)-1);
   assert_cmpint(rpc.insert.opcode, ==, MONGOC_OPCODE_INSERT);
   assert_cmpint(rpc.insert.flags, ==, MONGOC_INSERT_CONTINUE_ON_ERROR);
   assert(!strcmp("test.test", rpc.insert.collection));
   reader = bson_reader_new_from_data(rpc.insert.documents, rpc.insert.documents_len);
   while ((b = bson_reader_read(reader, &eof))) {
      r = bson_equal(b, &empty);
      assert(r);
      count++;
   }
   assert(eof == TRUE);
   assert(count == 20);

   assert_rpc_equal("insert1.dat", &rpc);
   bson_free(data);
   bson_reader_destroy(reader);
   bson_destroy(&empty);
}


static void
test_mongoc_rpc_kill_cursors_gather (void)
{
   mongoc_rpc_t rpc;
   bson_int64_t cursors[] = { BSON_UINT64_TO_LE(1),
                              BSON_UINT64_TO_LE(2),
                              BSON_UINT64_TO_LE(3),
                              BSON_UINT64_TO_LE(4),
                              BSON_UINT64_TO_LE(5) };

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   rpc.kill_cursors.msg_len = 0;
   rpc.kill_cursors.request_id = 1234;
   rpc.kill_cursors.response_to = -1;
   rpc.kill_cursors.opcode = MONGOC_OPCODE_KILL_CURSORS;
   rpc.kill_cursors.zero = 0;
   rpc.kill_cursors.n_cursors = 5;
   rpc.kill_cursors.cursors = cursors;

   assert_rpc_equal("kill_cursors1.dat", &rpc);
}


static void
test_mongoc_rpc_kill_cursors_scatter (void)
{
   bson_uint8_t *data;
   const bson_int64_t cursors[] = { BSON_UINT64_TO_LE(1),
                                    BSON_UINT64_TO_LE(2),
                                    BSON_UINT64_TO_LE(3),
                                    BSON_UINT64_TO_LE(4),
                                    BSON_UINT64_TO_LE(5) };
   mongoc_rpc_t rpc;
   bson_bool_t r;
   size_t length;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   data = get_test_file("kill_cursors1.dat", &length);
   r = mongoc_rpc_scatter(&rpc, data, length);
   assert(r);

   assert(BSON_UINT32_FROM_LE(rpc.kill_cursors.msg_len) == 64);
   assert(BSON_UINT32_FROM_LE(rpc.kill_cursors.request_id) == 1234);
   assert(BSON_UINT32_FROM_LE(rpc.kill_cursors.response_to) == -1);
   assert(BSON_UINT32_FROM_LE(rpc.kill_cursors.opcode) == MONGOC_OPCODE_KILL_CURSORS);
   assert(rpc.kill_cursors.zero == 0);
   assert(BSON_UINT32_FROM_LE(rpc.kill_cursors.n_cursors) == 5);
   assert(!memcmp(rpc.kill_cursors.cursors, cursors, 5 * 8));

   assert_rpc_equal("kill_cursors1.dat", &rpc);
   bson_free(data);
}


static void
test_mongoc_rpc_msg_gather (void)
{
   mongoc_rpc_t rpc;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   rpc.msg.msg_len = 0;
   rpc.msg.request_id = 1234;
   rpc.msg.response_to = -1;
   rpc.msg.opcode = MONGOC_OPCODE_MSG;
   rpc.msg.msg = "this is a test message.";

   assert_rpc_equal("msg1.dat", &rpc);
}


static void
test_mongoc_rpc_msg_scatter (void)
{
   bson_uint8_t *data;
   mongoc_rpc_t rpc;
   bson_bool_t r;
   size_t length;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   data = get_test_file("msg1.dat", &length);
   r = mongoc_rpc_scatter(&rpc, data, length);
   assert(r);

   assert(BSON_UINT32_FROM_LE(rpc.msg.msg_len) == 40);
   assert(BSON_UINT32_FROM_LE(rpc.msg.request_id) == 1234);
   assert(BSON_UINT32_FROM_LE(rpc.msg.response_to) == -1);
   assert(BSON_UINT32_FROM_LE(rpc.msg.opcode) == MONGOC_OPCODE_MSG);
   assert(!strcmp(rpc.msg.msg, "this is a test message."));

   assert_rpc_equal("msg1.dat", &rpc);
   bson_free(data);
}


static void
test_mongoc_rpc_query_gather (void)
{
   mongoc_rpc_t rpc;
   bson_t b;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   bson_init(&b);

   rpc.query.msg_len = 0;
   rpc.query.request_id = 1234;
   rpc.query.response_to = -1;
   rpc.query.opcode = MONGOC_OPCODE_QUERY;
   rpc.query.flags = MONGOC_QUERY_SLAVE_OK;
   rpc.query.collection = "test.test";
   rpc.query.skip = 5;
   rpc.query.n_return = 1;
   rpc.query.query = bson_get_data(&b);
   rpc.query.fields = bson_get_data(&b);

   assert_rpc_equal("query1.dat", &rpc);
}


static void
test_mongoc_rpc_query_scatter (void)
{
   bson_uint8_t *data;
   mongoc_rpc_t rpc;
   bson_bool_t r;
   bson_t empty;
   size_t length;

   bson_init(&empty);

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   data = get_test_file("query1.dat", &length);
   r = mongoc_rpc_scatter(&rpc, data, length);
   assert(r);

   assert(BSON_UINT32_FROM_LE(rpc.query.msg_len) == 48);
   assert(BSON_UINT32_FROM_LE(rpc.query.request_id) == 1234);
   assert(BSON_UINT32_FROM_LE(rpc.query.response_to) == (bson_uint32_t)-1);
   assert(BSON_UINT32_FROM_LE(rpc.query.opcode) == MONGOC_OPCODE_QUERY);
   assert(BSON_UINT32_FROM_LE(rpc.query.flags) == MONGOC_QUERY_SLAVE_OK);
   assert(!strcmp(rpc.query.collection, "test.test"));
   assert(BSON_UINT32_FROM_LE(rpc.query.skip) == 5);
   assert(BSON_UINT32_FROM_LE(rpc.query.n_return) == 1);
   assert(!memcmp(rpc.query.query, bson_get_data(&empty), 5));
   assert(!memcmp(rpc.query.fields, bson_get_data(&empty), 5));

   assert_rpc_equal("query1.dat", &rpc);
   bson_free(data);
}


static void
test_mongoc_rpc_reply_gather (void)
{
   bson_writer_t *writer;
   mongoc_rpc_t rpc;
   bson_uint8_t *buf = NULL;
   size_t len = 0;
   bson_t *b;
   int i;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   writer = bson_writer_new(&buf, &len, 0, bson_realloc);
   for (i = 0; i < 100; i++) {
      bson_writer_begin(writer, &b);
      bson_writer_end(writer);
   }

   rpc.reply.msg_len = 0;
   rpc.reply.request_id = 1234;
   rpc.reply.response_to = -1;
   rpc.reply.opcode = MONGOC_OPCODE_REPLY;
   rpc.reply.flags = MONGOC_REPLY_AWAIT_CAPABLE;
   rpc.reply.cursor_id = 12345678;
   rpc.reply.start_from = 50;
   rpc.reply.n_returned = 100;
   rpc.reply.documents = buf;
   rpc.reply.documents_len = bson_writer_get_length(writer);

   assert_rpc_equal("reply1.dat", &rpc);
   bson_writer_destroy(writer);
   bson_free(buf);
}


static void
test_mongoc_rpc_reply_scatter (void)
{
   bson_reader_t *reader;
   bson_uint8_t *data;
   mongoc_rpc_t rpc;
   const bson_t *b;
   bson_bool_t r;
   bson_bool_t eof = FALSE;
   bson_t empty;
   size_t length;
   int count = 0;

   bson_init(&empty);

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   data = get_test_file("reply1.dat", &length);
   r = mongoc_rpc_scatter(&rpc, data, length);
   assert(r);

   assert(BSON_UINT32_FROM_LE(rpc.reply.msg_len) == 536);
   assert(BSON_UINT32_FROM_LE(rpc.reply.request_id) == 1234);
   assert(BSON_UINT32_FROM_LE(rpc.reply.response_to) == -1);
   assert(BSON_UINT32_FROM_LE(rpc.reply.opcode) == MONGOC_OPCODE_REPLY);
   assert(BSON_UINT32_FROM_LE(rpc.reply.flags) == MONGOC_REPLY_AWAIT_CAPABLE);
   assert(BSON_UINT64_FROM_LE(rpc.reply.cursor_id) == 12345678);
   assert(BSON_UINT32_FROM_LE(rpc.reply.start_from) == 50);
   assert(BSON_UINT32_FROM_LE(rpc.reply.n_returned) == 100);
   assert(rpc.reply.documents_len == 500);
   reader = bson_reader_new_from_data(rpc.reply.documents, rpc.reply.documents_len);
   while ((b = bson_reader_read(reader, &eof))) {
      r = bson_equal(b, &empty);
      assert(r);
      count++;
   }
   assert(eof == TRUE);
   assert(count == 100);

   assert_rpc_equal("reply1.dat", &rpc);
   bson_reader_destroy(reader);
   bson_free(data);
}


static void
test_mongoc_rpc_reply_scatter2 (void)
{
   bson_reader_t *reader;
   bson_uint8_t *data;
   mongoc_rpc_t rpc;
   const bson_t *b;
   bson_bool_t r;
   bson_bool_t eof = FALSE;
   bson_t empty;
   size_t length;
   int count = 0;

   bson_init(&empty);

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   data = get_test_file("reply2.dat", &length);
   r = mongoc_rpc_scatter(&rpc, data, length);
   assert(r);

   assert(BSON_UINT32_FROM_LE(rpc.reply.msg_len) == 16236);
   assert(BSON_UINT32_FROM_LE(rpc.reply.request_id) == 0);
   assert(BSON_UINT32_FROM_LE(rpc.reply.response_to) == 1234);
   assert(BSON_UINT32_FROM_LE(rpc.reply.opcode) == MONGOC_OPCODE_REPLY);
   assert(BSON_UINT32_FROM_LE(rpc.reply.flags) == 0);
   assert(BSON_UINT64_FROM_LE(rpc.reply.cursor_id) == 12345678);
   assert(BSON_UINT32_FROM_LE(rpc.reply.start_from) == 0);
   assert(BSON_UINT32_FROM_LE(rpc.reply.n_returned) == 100);
   assert(rpc.reply.documents_len == 16200);
   reader = bson_reader_new_from_data(rpc.reply.documents, rpc.reply.documents_len);
   while ((b = bson_reader_read(reader, &eof))) {
      count++;
   }
   assert(eof == TRUE);
   assert(count == 100);

   assert_rpc_equal("reply2.dat", &rpc);
   bson_reader_destroy(reader);
   bson_free(data);
}


static void
test_mongoc_rpc_update_gather (void)
{
   mongoc_rpc_t rpc;
   bson_t sel;
   bson_t up;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   bson_init(&sel);
   bson_init(&up);

   rpc.update.msg_len = 0;
   rpc.update.request_id = 1234;
   rpc.update.response_to = -1;
   rpc.update.opcode = MONGOC_OPCODE_UPDATE;
   rpc.update.zero = 0;
   rpc.update.collection = "test.test";
   rpc.update.flags = MONGOC_UPDATE_MULTI_UPDATE;
   rpc.update.selector = bson_get_data(&sel);
   rpc.update.update = bson_get_data(&up);

   assert_rpc_equal("update1.dat", &rpc);
}


static void
test_mongoc_rpc_update_scatter (void)
{
   bson_uint8_t *data;
   mongoc_rpc_t rpc;
   bson_bool_t r;
   bson_t b;
   bson_t empty;
   size_t length;
   bson_int32_t len;

   bson_init(&empty);

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   data = get_test_file("update1.dat", &length);
   r = mongoc_rpc_scatter(&rpc, data, length);
   assert(r);

   assert(BSON_UINT32_FROM_LE(rpc.update.msg_len) == 44);
   assert(BSON_UINT32_FROM_LE(rpc.update.request_id) == 1234);
   assert(BSON_UINT32_FROM_LE(rpc.update.response_to) == -1);
   assert(BSON_UINT32_FROM_LE(rpc.update.opcode) == MONGOC_OPCODE_UPDATE);
   assert(BSON_UINT32_FROM_LE(rpc.update.flags) == MONGOC_UPDATE_MULTI_UPDATE);
   assert(!strcmp(rpc.update.collection, "test.test"));

   memcpy(&len, rpc.update.selector, 4);
   len = BSON_UINT64_TO_LE(len);
   assert(len > 4);
   r = bson_init_static(&b, rpc.update.selector, len);
   assert(r);
   r = bson_equal(&b, &empty);
   assert(r);
   bson_destroy(&b);

   memcpy(&len, rpc.update.update, 4);
   len = BSON_UINT64_TO_LE(len);
   assert(len > 4);
   r = bson_init_static(&b, rpc.update.update, len);
   assert(r);
   r = bson_equal(&b, &empty);
   assert(r);
   bson_destroy(&b);

   assert_rpc_equal("update1.dat", &rpc);
   bson_free(data);
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/rpc/delete/gather", test_mongoc_rpc_delete_gather);
   run_test("/mongoc/rpc/delete/scatter", test_mongoc_rpc_delete_scatter);
   run_test("/mongoc/rpc/get_more/gather", test_mongoc_rpc_get_more_gather);
   run_test("/mongoc/rpc/get_more/scatter", test_mongoc_rpc_get_more_scatter);
   run_test("/mongoc/rpc/insert/gather", test_mongoc_rpc_insert_gather);
   run_test("/mongoc/rpc/insert/scatter", test_mongoc_rpc_insert_scatter);
   run_test("/mongoc/rpc/kill_cursors/gather", test_mongoc_rpc_kill_cursors_gather);
   run_test("/mongoc/rpc/kill_cursors/scatter", test_mongoc_rpc_kill_cursors_scatter);
   run_test("/mongoc/rpc/msg/gather", test_mongoc_rpc_msg_gather);
   run_test("/mongoc/rpc/msg/scatter", test_mongoc_rpc_msg_scatter);
   run_test("/mongoc/rpc/query/gather", test_mongoc_rpc_query_gather);
   run_test("/mongoc/rpc/query/scatter", test_mongoc_rpc_query_scatter);
   run_test("/mongoc/rpc/reply/gather", test_mongoc_rpc_reply_gather);
   run_test("/mongoc/rpc/reply/scatter", test_mongoc_rpc_reply_scatter);
   run_test("/mongoc/rpc/reply/scatter2", test_mongoc_rpc_reply_scatter2);
   run_test("/mongoc/rpc/update/gather", test_mongoc_rpc_update_gather);
   run_test("/mongoc/rpc/update/scatter", test_mongoc_rpc_update_scatter);

   return 0;
}
