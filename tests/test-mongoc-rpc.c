#include <fcntl.h>
#include <mongoc.h>
#include <mongoc-array-private.h>
#include <mongoc-rpc-private.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "TestSuite.h"


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
   ASSERT(len > 0);

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
#define IOVEC_ARRAY_FIELD(_name) \
   do { \
      mongoc_array_t a_buf; \
      mongoc_array_t b_buf; \
      _mongoc_array_init(&a_buf, 1); \
      _mongoc_array_init(&b_buf, 1); \
      size_t _i; \
      bson_bool_t is_equal; \
      for (_i = 0; _i < a->n_##_name; _i++) { \
         _mongoc_array_append_vals(&a_buf, a->_name[_i].iov_base, a->_name[_i].iov_len); \
      } \
      for (_i = 0; _i < b->n_##_name; _i++) { \
         _mongoc_array_append_vals(&b_buf, b->_name[_i].iov_base, b->_name[_i].iov_len); \
      } \
      is_equal = (a_buf.len == b_buf.len) \
              && (memcmp(a_buf.data, b_buf.data, a_buf.len) == 0); \
      _mongoc_array_destroy(&a_buf); \
      _mongoc_array_destroy(&b_buf); \
      return is_equal; \
   } while(0);
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
#undef IOVEC_ARRAY_FIELD


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
   _mongoc_array_init(&ar, sizeof(struct iovec));

   /*
    * Gather our RPC into a series of iovec that can be compared
    * to the buffer from the RCP snapshot file.
    */
   _mongoc_rpc_gather(rpc, &ar);

#if 0
   printf("Before swabbing\n");
   printf("=========================\n");
   mongoc_rpc_printf(rpc);
#endif

   _mongoc_rpc_swab_to_le(rpc);

#if 0
   printf("After swabbing\n");
   printf("=========================\n");
   mongoc_rpc_printf(rpc);
#endif

   for (i = 0; i < ar.len; i++) {
      iov = &_mongoc_array_index(&ar, struct iovec, i);
      ASSERT(iov->iov_len <= (length - off));
      r = memcmp(&data[off], iov->iov_base, iov->iov_len);
      if (r) {
         printf("\nError iovec: %u\n", i);
      }
      ASSERT(r == 0);
      off += iov->iov_len;
   }

   _mongoc_array_destroy(&ar);
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
   r = _mongoc_rpc_scatter(&rpc, data, length);
   ASSERT(r);
   _mongoc_rpc_swab_from_le(&rpc);

   ASSERT_CMPINT(rpc.delete.msg_len, ==, 39);
   ASSERT_CMPINT(rpc.delete.request_id, ==, 1234);
   ASSERT_CMPINT(rpc.delete.response_to, ==, -1);
   ASSERT_CMPINT(rpc.delete.opcode, ==, MONGOC_OPCODE_DELETE);
   ASSERT_CMPINT(rpc.delete.zero, ==, 0);
   ASSERT(!strcmp("test.test", rpc.delete.collection));
   ASSERT_CMPINT(rpc.delete.flags, ==, MONGOC_DELETE_SINGLE_REMOVE);
   ASSERT(!memcmp(rpc.delete.selector, bson_get_data(&sel), sel.len));

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
   r = _mongoc_rpc_scatter(&rpc, data, length);
   ASSERT(r);
   _mongoc_rpc_swab_from_le(&rpc);

   ASSERT(rpc.get_more.msg_len == 42);
   ASSERT(rpc.get_more.request_id == 1234);
   ASSERT(rpc.get_more.response_to == -1);
   ASSERT(rpc.get_more.opcode == MONGOC_OPCODE_GET_MORE);
   ASSERT(rpc.get_more.zero == 0);
   ASSERT(!strcmp("test.test", rpc.get_more.collection));
   ASSERT(rpc.get_more.n_return == 5);
   ASSERT(rpc.get_more.cursor_id == 12345678);

   assert_rpc_equal("get_more1.dat", &rpc);
   bson_free(data);
}


static void
test_mongoc_rpc_insert_gather (void)
{
   mongoc_rpc_t rpc;
   struct iovec iov[20];
   bson_t b;
   int i;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   bson_init(&b);

   for (i = 0; i < 20; i++) {
      iov[i].iov_base = (void *)bson_get_data(&b);
      iov[i].iov_len = b.len;
   }

   rpc.insert.msg_len = 0;
   rpc.insert.request_id = 1234;
   rpc.insert.response_to = -1;
   rpc.insert.opcode = MONGOC_OPCODE_INSERT;
   rpc.insert.flags = MONGOC_INSERT_CONTINUE_ON_ERROR;
   rpc.insert.collection = "test.test";
   rpc.insert.documents = iov;
   rpc.insert.n_documents = 20;

   assert_rpc_equal("insert1.dat", &rpc);
   bson_destroy(&b);
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
   r = _mongoc_rpc_scatter(&rpc, data, length);
   ASSERT(r);
   _mongoc_rpc_swab_from_le(&rpc);

   ASSERT_CMPINT(rpc.insert.msg_len, ==, 130);
   ASSERT_CMPINT(rpc.insert.request_id, ==, 1234);
   ASSERT_CMPINT(rpc.insert.response_to, ==, (bson_uint32_t)-1);
   ASSERT_CMPINT(rpc.insert.opcode, ==, MONGOC_OPCODE_INSERT);
   ASSERT_CMPINT(rpc.insert.flags, ==, MONGOC_INSERT_CONTINUE_ON_ERROR);
   ASSERT(!strcmp("test.test", rpc.insert.collection));
   reader = bson_reader_new_from_data(rpc.insert.documents[0].iov_base, rpc.insert.documents[0].iov_len);
   while ((b = bson_reader_read(reader, &eof))) {
      r = bson_equal(b, &empty);
      ASSERT(r);
      count++;
   }
   ASSERT(eof == TRUE);
   ASSERT(count == 20);

   assert_rpc_equal("insert1.dat", &rpc);
   bson_free(data);
   bson_reader_destroy(reader);
   bson_destroy(&empty);
}


static void
test_mongoc_rpc_kill_cursors_gather (void)
{
   mongoc_rpc_t rpc;
   bson_int64_t cursors[] = { 1, 2, 3, 4, 5 };

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
   const bson_int64_t cursors[] = { 1, 2, 3, 4, 5 };
   mongoc_rpc_t rpc;
   bson_bool_t r;
   size_t length;

   memset(&rpc, 0xFFFFFFFF, sizeof rpc);

   data = get_test_file("kill_cursors1.dat", &length);
   r = _mongoc_rpc_scatter(&rpc, data, length);
   ASSERT(r);
   _mongoc_rpc_swab_from_le(&rpc);

   ASSERT_CMPINT(rpc.kill_cursors.msg_len, ==, 64);
   ASSERT_CMPINT(rpc.kill_cursors.request_id, ==, 1234);
   ASSERT_CMPINT(rpc.kill_cursors.response_to, ==, -1);
   ASSERT_CMPINT(rpc.kill_cursors.opcode, ==, MONGOC_OPCODE_KILL_CURSORS);
   ASSERT_CMPINT(rpc.kill_cursors.zero, ==, 0);
   ASSERT_CMPINT(rpc.kill_cursors.n_cursors, ==, 5);
   ASSERT(!memcmp(rpc.kill_cursors.cursors, cursors, 5 * 8));

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
   r = _mongoc_rpc_scatter(&rpc, data, length);
   ASSERT(r);
   _mongoc_rpc_swab_from_le(&rpc);

   ASSERT(rpc.msg.msg_len == 40);
   ASSERT(rpc.msg.request_id == 1234);
   ASSERT(rpc.msg.response_to == -1);
   ASSERT(rpc.msg.opcode == MONGOC_OPCODE_MSG);
   ASSERT(!strcmp(rpc.msg.msg, "this is a test message."));

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
   r = _mongoc_rpc_scatter(&rpc, data, length);
   ASSERT(r);
   _mongoc_rpc_swab_from_le(&rpc);

   ASSERT(rpc.query.msg_len == 48);
   ASSERT(rpc.query.request_id == 1234);
   ASSERT(rpc.query.response_to == (bson_uint32_t)-1);
   ASSERT(rpc.query.opcode == MONGOC_OPCODE_QUERY);
   ASSERT(rpc.query.flags == MONGOC_QUERY_SLAVE_OK);
   ASSERT(!strcmp(rpc.query.collection, "test.test"));
   ASSERT(rpc.query.skip == 5);
   ASSERT(rpc.query.n_return == 1);
   ASSERT(!memcmp(rpc.query.query, bson_get_data(&empty), 5));
   ASSERT(!memcmp(rpc.query.fields, bson_get_data(&empty), 5));

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
   r = _mongoc_rpc_scatter(&rpc, data, length);
   ASSERT(r);
   _mongoc_rpc_swab_from_le(&rpc);

   ASSERT_CMPINT(rpc.reply.msg_len, ==, 536);
   ASSERT_CMPINT(rpc.reply.request_id, ==, 1234);
   ASSERT_CMPINT(rpc.reply.response_to, ==, -1);
   ASSERT_CMPINT(rpc.reply.opcode, ==, MONGOC_OPCODE_REPLY);
   ASSERT_CMPINT(rpc.reply.flags, ==, MONGOC_REPLY_AWAIT_CAPABLE);
   ASSERT(rpc.reply.cursor_id == 12345678LL);
   ASSERT_CMPINT(rpc.reply.start_from, ==, 50);
   ASSERT_CMPINT(rpc.reply.n_returned, ==, 100);
   ASSERT_CMPINT(rpc.reply.documents_len, ==, 500);
   reader = bson_reader_new_from_data(rpc.reply.documents, rpc.reply.documents_len);
   while ((b = bson_reader_read(reader, &eof))) {
      r = bson_equal(b, &empty);
      ASSERT(r);
      count++;
   }
   ASSERT(eof == TRUE);
   ASSERT(count == 100);

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
   r = _mongoc_rpc_scatter(&rpc, data, length);
   ASSERT(r);
   _mongoc_rpc_swab_from_le(&rpc);

   ASSERT(rpc.reply.msg_len == 16236);
   ASSERT(rpc.reply.request_id == 0);
   ASSERT(rpc.reply.response_to == 1234);
   ASSERT(rpc.reply.opcode == MONGOC_OPCODE_REPLY);
   ASSERT(rpc.reply.flags == 0);
   ASSERT(rpc.reply.cursor_id == 12345678);
   ASSERT(rpc.reply.start_from == 0);
   ASSERT(rpc.reply.n_returned == 100);
   ASSERT(rpc.reply.documents_len == 16200);
   reader = bson_reader_new_from_data(rpc.reply.documents, rpc.reply.documents_len);
   while ((b = bson_reader_read(reader, &eof))) {
      count++;
   }
   ASSERT(eof == TRUE);
   ASSERT(count == 100);

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
   r = _mongoc_rpc_scatter(&rpc, data, length);
   ASSERT(r);
   _mongoc_rpc_swab_from_le(&rpc);

   ASSERT_CMPINT(rpc.update.msg_len, ==, 44);
   ASSERT_CMPINT(rpc.update.request_id, ==, 1234);
   ASSERT_CMPINT(rpc.update.response_to, ==, -1);
   ASSERT_CMPINT(rpc.update.opcode, ==, MONGOC_OPCODE_UPDATE);
   ASSERT_CMPINT(rpc.update.flags, ==, MONGOC_UPDATE_MULTI_UPDATE);
   ASSERT(!strcmp(rpc.update.collection, "test.test"));

   memcpy(&len, rpc.update.selector, 4);
   len = BSON_UINT32_FROM_LE(len);
   ASSERT(len > 4);
   r = bson_init_static(&b, rpc.update.selector, len);
   ASSERT(r);
   r = bson_equal(&b, &empty);
   ASSERT(r);
   bson_destroy(&b);

   memcpy(&len, rpc.update.update, 4);
   len = BSON_UINT32_FROM_LE(len);
   ASSERT(len > 4);
   r = bson_init_static(&b, rpc.update.update, len);
   ASSERT(r);
   r = bson_equal(&b, &empty);
   ASSERT(r);
   bson_destroy(&b);

   assert_rpc_equal("update1.dat", &rpc);
   bson_free(data);
}


void
test_rpc_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Rpc/delete/gather", test_mongoc_rpc_delete_gather);
   TestSuite_Add (suite, "/Rpc/delete/scatter", test_mongoc_rpc_delete_scatter);
   TestSuite_Add (suite, "/Rpc/get_more/gather", test_mongoc_rpc_get_more_gather);
   TestSuite_Add (suite, "/Rpc/get_more/scatter", test_mongoc_rpc_get_more_scatter);
   TestSuite_Add (suite, "/Rpc/insert/gather", test_mongoc_rpc_insert_gather);
   TestSuite_Add (suite, "/Rpc/insert/scatter", test_mongoc_rpc_insert_scatter);
   TestSuite_Add (suite, "/Rpc/kill_cursors/gather", test_mongoc_rpc_kill_cursors_gather);
   TestSuite_Add (suite, "/Rpc/kill_cursors/scatter", test_mongoc_rpc_kill_cursors_scatter);
   TestSuite_Add (suite, "/Rpc/msg/gather", test_mongoc_rpc_msg_gather);
   TestSuite_Add (suite, "/Rpc/msg/scatter", test_mongoc_rpc_msg_scatter);
   TestSuite_Add (suite, "/Rpc/query/gather", test_mongoc_rpc_query_gather);
   TestSuite_Add (suite, "/Rpc/query/scatter", test_mongoc_rpc_query_scatter);
   TestSuite_Add (suite, "/Rpc/reply/gather", test_mongoc_rpc_reply_gather);
   TestSuite_Add (suite, "/Rpc/reply/scatter", test_mongoc_rpc_reply_scatter);
   TestSuite_Add (suite, "/Rpc/reply/scatter2", test_mongoc_rpc_reply_scatter2);
   TestSuite_Add (suite, "/Rpc/update/gather", test_mongoc_rpc_update_gather);
   TestSuite_Add (suite, "/Rpc/update/scatter", test_mongoc_rpc_update_scatter);
}
