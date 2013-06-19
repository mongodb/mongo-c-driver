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
test_mongoc_rpc_delete (void)
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
   rpc.delete.selector = &sel;

   assert_rpc_equal("delete1.dat", &rpc);
}


static void
test_mongoc_rpc_get_more (void)
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
test_mongoc_rpc_insert (void)
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
test_mongoc_rpc_kill_cursors (void)
{
   mongoc_rpc_t rpc;
   bson_int64_t cursors[] = { 1, 2, 3, 4, 5 };

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
test_mongoc_rpc_msg (void)
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
test_mongoc_rpc_query (void)
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
   rpc.query.query = &b;
   rpc.query.fields = &b;

   assert_rpc_equal("query1.dat", &rpc);
}


static void
test_mongoc_rpc_reply (void)
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
test_mongoc_rpc_update (void)
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
   rpc.update.selector = &sel;
   rpc.update.update = &up;

   assert_rpc_equal("update1.dat", &rpc);
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/rpc/delete", test_mongoc_rpc_delete);
   run_test("/mongoc/rpc/get_more", test_mongoc_rpc_get_more);
   run_test("/mongoc/rpc/insert", test_mongoc_rpc_insert);
   run_test("/mongoc/rpc/kill_cursors", test_mongoc_rpc_kill_cursors);
   run_test("/mongoc/rpc/msg", test_mongoc_rpc_msg);
   run_test("/mongoc/rpc/query", test_mongoc_rpc_query);
   run_test("/mongoc/rpc/reply", test_mongoc_rpc_reply);
   run_test("/mongoc/rpc/update", test_mongoc_rpc_update);

   return 0;
}
