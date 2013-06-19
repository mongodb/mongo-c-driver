#include <fcntl.h>
#include <mongoc.h>
#include <mongoc-array-private.h>
#include <mongoc-event-private.h>
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

   len = 4096;
   buf = bson_malloc0(len);
   len = read(fd, buf, len);
   assert(len > 0);

   *length = len;
   return buf;
}


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
   mongoc_rpc_gather(rpc, &ar);
#if 0
   mongoc_rpc_printf(rpc);
#endif
   mongoc_rpc_swab(rpc);

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
   rpc.delete.op_code = MONGOC_OPCODE_DELETE;
   rpc.delete.zero = 0;
   snprintf(rpc.delete.collection, sizeof rpc.delete.collection, "%s", "test.test");
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

   assert(rpc.delete.msg_len == 39);
   assert(rpc.delete.request_id == 1234);
   assert(rpc.delete.response_to == -1);
   assert(rpc.delete.op_code == MONGOC_OPCODE_DELETE);
   assert(rpc.delete.zero == 0);
   assert(!strcmp("test.test", rpc.delete.collection));
   assert(rpc.delete.flags == MONGOC_DELETE_SINGLE_REMOVE);
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
   rpc.get_more.op_code = MONGOC_OPCODE_GET_MORE;
   rpc.get_more.zero = 0;
   snprintf(rpc.get_more.collection, sizeof rpc.get_more.collection, "%s", "test.test");
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

   assert(rpc.get_more.msg_len == 42);
   assert(rpc.get_more.request_id == 1234);
   assert(rpc.get_more.response_to == -1);
   assert(rpc.get_more.op_code == MONGOC_OPCODE_GET_MORE);
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
   rpc.insert.op_code = MONGOC_OPCODE_INSERT;
   rpc.insert.flags = MONGOC_INSERT_CONTINUE_ON_ERROR;
   snprintf(rpc.insert.collection, sizeof rpc.insert.collection, "%s", "test.test");
   rpc.insert.documents = buf;
   rpc.insert.documents_len = bson_writer_get_length(writer);

   assert_rpc_equal("insert1.dat", &rpc);
   bson_writer_destroy(writer);
   bson_free(buf);
}


static void
test_mongoc_rpc_insert_scatter (void)
{
   bson_reader_t reader;
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

   assert(rpc.insert.msg_len == 130);
   assert(rpc.insert.request_id == 1234);
   assert(rpc.insert.response_to == -1);
   assert(rpc.insert.op_code == MONGOC_OPCODE_INSERT);
   assert(rpc.insert.flags == MONGOC_INSERT_CONTINUE_ON_ERROR);
   assert(!strcmp("test.test", rpc.insert.collection));
   bson_reader_init_from_data(&reader, rpc.insert.documents, rpc.insert.documents_len);
   while ((b = bson_reader_read(&reader, &eof))) {
      r = bson_equal(b, &empty);
      assert(r);
      count++;
   }
   assert(eof == TRUE);
   assert(count == 20);

   assert_rpc_equal("insert1.dat", &rpc);
   bson_free(data);
   bson_reader_destroy(&reader);
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
   rpc.kill_cursors.op_code = MONGOC_OPCODE_KILL_CURSORS;
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

   assert(rpc.kill_cursors.msg_len == 64);
   assert(rpc.kill_cursors.request_id == 1234);
   assert(rpc.kill_cursors.response_to == -1);
   assert(rpc.kill_cursors.op_code == MONGOC_OPCODE_KILL_CURSORS);
   assert(rpc.kill_cursors.zero == 0);
   assert(rpc.kill_cursors.n_cursors == 5);
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
   rpc.msg.op_code = MONGOC_OPCODE_MSG;
   snprintf(rpc.msg.msg, sizeof rpc.msg.msg, "%s",
            "this is a test message.");

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

   assert(rpc.msg.msg_len == 40);
   assert(rpc.msg.request_id == 1234);
   assert(rpc.msg.response_to == -1);
   assert(rpc.msg.op_code == MONGOC_OPCODE_MSG);
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
   rpc.query.op_code = MONGOC_OPCODE_QUERY;
   rpc.query.flags = MONGOC_QUERY_SLAVE_OK;
   snprintf(rpc.query.collection, sizeof rpc.query.collection, "%s", "test.test");
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

   assert(rpc.query.msg_len == 48);
   assert(rpc.query.request_id == 1234);
   assert(rpc.query.response_to == -1);
   assert(rpc.query.op_code == MONGOC_OPCODE_QUERY);
   assert(rpc.query.flags == MONGOC_QUERY_SLAVE_OK);
   assert(!strcmp(rpc.query.collection, "test.test"));
   assert(rpc.query.skip == 5);
   assert(rpc.query.n_return == 1);
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
   rpc.reply.op_code = MONGOC_OPCODE_REPLY;
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
   bson_reader_t reader;
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

   assert(rpc.reply.msg_len == 536);
   assert(rpc.reply.request_id == 1234);
   assert(rpc.reply.response_to == -1);
   assert(rpc.reply.op_code == MONGOC_OPCODE_REPLY);
   assert(rpc.reply.flags == MONGOC_REPLY_AWAIT_CAPABLE);
   assert(rpc.reply.cursor_id == 12345678);
   assert(rpc.reply.start_from == 50);
   assert(rpc.reply.n_returned == 100);
   assert(rpc.reply.documents_len == 500);
   bson_reader_init_from_data(&reader, rpc.reply.documents, rpc.reply.documents_len);
   while ((b = bson_reader_read(&reader, &eof))) {
      r = bson_equal(b, &empty);
      assert(r);
      count++;
   }
   assert(eof == TRUE);
   assert(count == 100);

   assert_rpc_equal("reply1.dat", &rpc);
   bson_reader_destroy(&reader);
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
   rpc.update.op_code = MONGOC_OPCODE_UPDATE;
   rpc.update.zero = 0;
   snprintf(rpc.update.collection, sizeof rpc.update.collection,
            "%s", "test.test");
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

   assert(rpc.update.msg_len == 44);
   assert(rpc.update.request_id == 1234);
   assert(rpc.update.response_to == -1);
   assert(rpc.update.op_code == MONGOC_OPCODE_UPDATE);
   assert(rpc.update.flags == MONGOC_UPDATE_MULTI_UPDATE);
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
   run_test("/mongoc/rpc/update/gather", test_mongoc_rpc_update_gather);
   run_test("/mongoc/rpc/update/scatter", test_mongoc_rpc_update_scatter);

   return 0;
}
