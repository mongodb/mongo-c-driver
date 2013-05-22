#include <fcntl.h>
#include <mongoc.h>
#include <mongoc-event-private.h>
#include <stdio.h>
#include <stdlib.h>
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
test_mongoc_event_query (void)
{
   mongoc_event_t q = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_QUERY);
   bson_uint8_t *buf = NULL;
   bson_uint8_t *fbuf = NULL;
   bson_error_t error;
   size_t buflen = 0;
   size_t fbuflen = 0;
   bson_t b;

   assert(q.type == MONGOC_OPCODE_QUERY);
   bson_init(&b);
   q.any.opcode = q.type;
   q.any.request_id = 1234;
   q.any.response_to = -1;
   q.query.query = &b;
   q.query.fields = &b;
   q.query.skip = 5;
   q.query.n_return = 1;
   q.query.flags = MONGOC_QUERY_SLAVE_OK;
   q.query.ns = "test.test";
   q.query.nslen = sizeof "test.test" - 1;
   mongoc_event_encode(&q, &buf, &buflen, NULL, &error);
   assert(buflen == 48);
   fbuf = get_test_file("query1.dat", &fbuflen);
   assert(buflen == fbuflen);
   assert(!memcmp(buf, fbuf, 48));
   bson_free(buf);
   bson_free(fbuf);
}


static void
test_mongoc_event_query_no_fields (void)
{
   mongoc_event_t q = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_QUERY);
   bson_uint8_t *buf = NULL;
   bson_uint8_t *fbuf = NULL;
   bson_error_t error;
   size_t buflen = 0;
   size_t fbuflen = 0;
   bson_t b;

   assert(q.type == MONGOC_OPCODE_QUERY);
   bson_init(&b);
   q.any.opcode = q.type;
   q.any.request_id = 1234;
   q.any.response_to = -1;
   q.query.query = &b;
   q.query.fields = NULL;
   q.query.skip = 5;
   q.query.n_return = 1;
   q.query.flags = MONGOC_QUERY_SLAVE_OK;
   q.query.ns = "test.test";
   q.query.nslen = sizeof "test.test" - 1;
   mongoc_event_encode(&q, &buf, &buflen, NULL, &error);
   assert(buflen == 43);
   fbuf = get_test_file("query2.dat", &fbuflen);
   assert(buflen == fbuflen);
   assert(!memcmp(buf, fbuf, 43));
   bson_free(buf);
   bson_free(fbuf);
}


static void
test_mongoc_event_insert (void)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_INSERT);
   bson_uint8_t *buf = NULL;
   bson_uint8_t *fbuf = NULL;
   bson_error_t error;
   size_t buflen = 0;
   size_t fbuflen = 0;
   bson_t b;
   bson_t **docs;
   unsigned i;

   bson_init(&b);

   docs = alloca(sizeof(bson_t*) * 20);
   for (i = 0; i < 20; i++) {
      docs[i] = &b;
   }

   assert(ev.type == MONGOC_OPCODE_INSERT);
   ev.any.opcode = ev.type;
   ev.any.request_id = 1234;
   ev.any.response_to = -1;
   ev.insert.flags = MONGOC_INSERT_CONTINUE_ON_ERROR;
   ev.insert.ns = "test.test";
   ev.insert.nslen = sizeof "test.test" - 1;
   ev.insert.docslen = 20;
   ev.insert.docs = docs;
   mongoc_event_encode(&ev, &buf, &buflen, NULL, &error);
   assert(buflen == 130);
   fbuf = get_test_file("insert1.dat", &fbuflen);
   assert(buflen == fbuflen);
   assert(!memcmp(buf, fbuf, 130));
   bson_free(buf);
   bson_free(fbuf);
}


static void
test_mongoc_event_update (void)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_UPDATE);
   bson_uint8_t *buf = NULL;
   bson_uint8_t *fbuf = NULL;
   bson_error_t error;
   size_t buflen = 0;
   size_t fbuflen = 0;
   bson_t sel;
   bson_t up;

   bson_init(&sel);
   bson_init(&up);

   assert(ev.type == MONGOC_OPCODE_UPDATE);
   ev.any.opcode = ev.type;
   ev.any.request_id = 1234;
   ev.any.response_to = -1;
   ev.update.ns = "test.test";
   ev.update.nslen = sizeof "test.test" - 1;
   ev.update.flags = MONGOC_UPDATE_MULTI_UPDATE;
   ev.update.selector = &sel;
   ev.update.update = &up;
   mongoc_event_encode(&ev, &buf, &buflen, NULL, &error);
   assert(buflen == 44);
   fbuf = get_test_file("update1.dat", &fbuflen);
   assert(buflen == fbuflen);
   assert(!memcmp(buf, fbuf, 44));
   bson_free(buf);
   bson_free(fbuf);
}


static void
test_mongoc_event_delete (void)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_DELETE);
   bson_uint8_t *buf = NULL;
   bson_uint8_t *fbuf = NULL;
   bson_error_t error;
   size_t buflen = 0;
   size_t fbuflen = 0;
   bson_t sel;

   bson_init(&sel);

   assert(ev.type == MONGOC_OPCODE_DELETE);
   ev.any.opcode = ev.type;
   ev.any.request_id = 1234;
   ev.any.response_to = -1;
   ev.delete.ns = "test.test";
   ev.delete.nslen = sizeof "test.test" - 1;
   ev.delete.flags = MONGOC_DELETE_SINGLE_REMOVE;
   ev.delete.selector = &sel;
   mongoc_event_encode(&ev, &buf, &buflen, NULL, &error);
   assert(buflen == 39);
   fbuf = get_test_file("delete1.dat", &fbuflen);
   assert(buflen == fbuflen);
   assert(!memcmp(buf, fbuf, 39));
   bson_free(buf);
   bson_free(fbuf);
}


static void
test_mongoc_event_get_more (void)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_GET_MORE);
   bson_uint8_t *buf = NULL;
   bson_uint8_t *fbuf = NULL;
   bson_error_t error;
   size_t buflen = 0;
   size_t fbuflen = 0;
   bson_t sel;

   bson_init(&sel);

   assert(ev.type == MONGOC_OPCODE_GET_MORE);
   ev.any.opcode = ev.type;
   ev.any.request_id = 1234;
   ev.any.response_to = -1;
   ev.get_more.ns = "test.test";
   ev.get_more.nslen = sizeof "test.test" - 1;
   ev.get_more.n_return = 5;
   ev.get_more.cursor_id = 12345678;
   mongoc_event_encode(&ev, &buf, &buflen, NULL, &error);
   assert(buflen == 42);
   fbuf = get_test_file("get_more1.dat", &fbuflen);
   assert(buflen == fbuflen);
   assert(!memcmp(buf, fbuf, 42));
   bson_free(buf);
   bson_free(fbuf);
}


static void
test_mongoc_event_kill_cursors (void)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_KILL_CURSORS);
   bson_uint8_t *buf = NULL;
   bson_uint8_t *fbuf = NULL;
   bson_uint64_t *cursors;
   bson_error_t error;
   size_t buflen = 0;
   size_t fbuflen = 0;
   bson_t sel;

   bson_init(&sel);

   cursors = alloca(sizeof *cursors * 5);
   cursors[0] = 1;
   cursors[1] = 2;
   cursors[2] = 3;
   cursors[3] = 4;
   cursors[4] = 5;

   assert(ev.type == MONGOC_OPCODE_KILL_CURSORS);
   ev.any.opcode = ev.type;
   ev.any.request_id = 1234;
   ev.any.response_to = -1;
   ev.kill_cursors.n_cursors = 5;
   ev.kill_cursors.cursors = cursors;
   mongoc_event_encode(&ev, &buf, &buflen, NULL, &error);
   assert(buflen == 64);
   fbuf = get_test_file("kill_cursors1.dat", &fbuflen);
   assert(buflen == fbuflen);
   assert(!memcmp(buf, fbuf, 64));
   bson_free(buf);
   bson_free(fbuf);
}


static void
test_mongoc_event_msg (void)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_MSG);
   bson_uint8_t *buf = NULL;
   bson_uint8_t *fbuf = NULL;
   bson_error_t error;
   size_t buflen = 0;
   size_t fbuflen = 0;

   assert(ev.type == MONGOC_OPCODE_MSG);
   ev.any.opcode = ev.type;
   ev.any.request_id = 1234;
   ev.any.response_to = -1;
   ev.msg.msglen = sizeof "this is a test message." - 1;
   ev.msg.msg = "this is a test message.";
   mongoc_event_encode(&ev, &buf, &buflen, NULL, &error);
   assert(buflen == 40);
   fbuf = get_test_file("msg1.dat", &fbuflen);
   assert(buflen == fbuflen);
   assert(!memcmp(buf, fbuf, 40));
   bson_free(buf);
   bson_free(fbuf);
}


static void
test_mongoc_event_reply (void)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_REPLY);
   bson_uint32_t i;
   bson_uint8_t *buf = NULL;
   bson_uint8_t *fbuf = NULL;
   bson_error_t error;
   size_t buflen = 0;
   size_t fbuflen = 0;
   bson_t b;
   bson_t **docs;

   bson_init(&b);

   docs = alloca(sizeof(bson_t*) * 100);
   for (i = 0; i < 100; i++) {
      docs[i] = &b;
   }

   assert(ev.type == MONGOC_OPCODE_REPLY);
   ev.any.opcode = ev.type;
   ev.any.request_id = 1234;
   ev.any.response_to = -1;
   ev.reply.flags = MONGOC_REPLY_AWAIT_CAPABLE;
   ev.reply.cursor_id = 12345678;
   ev.reply.start_from = 50;
   ev.reply.n_returned = 100;
   ev.reply.docs = docs;
   ev.reply.docslen = 100;
   mongoc_event_encode(&ev, &buf, &buflen, NULL, &error);
   assert(buflen == 536);
   fbuf = get_test_file("reply1.dat", &fbuflen);
   assert(buflen == fbuflen);
   assert(!memcmp(buf, fbuf, 536));
   bson_free(buf);
   bson_free(fbuf);
}


static void
test_mongoc_event_decode_reply (void)
{
   mongoc_stream_t *stream;
   mongoc_event_t ev = { 0 };
   const bson_t *b;
   bson_error_t error = { 0 };
   bson_bool_t r;
   bson_bool_t eof = FALSE;
   bson_iter_t iter;
   int count = 0;
   int fd;

   fd = open("tests/binary/reply1.dat", O_RDONLY);
   assert(fd >= 0);

   stream = mongoc_stream_new_from_unix(fd);
   assert(stream);

   r = mongoc_event_read(&ev, stream, &error);
   assert_cmpint(r, ==, TRUE);

   assert(ev.any.type == MONGOC_OPCODE_REPLY);

   while ((b = bson_reader_read(&ev.reply.docs_reader, &eof))) {
      count++;
      assert(bson_iter_init(&iter, b));
      assert(!bson_iter_next(&iter));
   }

   assert(eof);
   assert_cmpint(count, ==, 100);

   mongoc_stream_close(stream);
}


static void
test_mongoc_event_decode_msg (void)
{
   mongoc_stream_t *stream;
   mongoc_event_t ev = { 0 };
   bson_error_t error = { 0 };
   bson_bool_t r;
   int fd;

   fd = open("tests/binary/msg1.dat", O_RDONLY);
   assert(fd >= 0);

   stream = mongoc_stream_new_from_unix(fd);
   assert(stream);

   r = mongoc_event_read(&ev, stream, &error);
   assert_cmpint(r, ==, TRUE);

   assert(ev.any.type == MONGOC_OPCODE_MSG);
   assert_cmpint(ev.msg.msglen, ==, 23);
   assert(!strcmp(ev.msg.msg, "this is a test message."));
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/event/encode/delete", test_mongoc_event_delete);
   run_test("/mongoc/event/encode/get_more", test_mongoc_event_get_more);
   run_test("/mongoc/event/encode/insert", test_mongoc_event_insert);
   run_test("/mongoc/event/encode/kill_cursors", test_mongoc_event_kill_cursors);
   run_test("/mongoc/event/encode/msg", test_mongoc_event_msg);
   run_test("/mongoc/event/encode/query", test_mongoc_event_query);
   run_test("/mongoc/event/encode/query_no_fields", test_mongoc_event_query_no_fields);
   run_test("/mongoc/event/encode/reply", test_mongoc_event_reply);
   run_test("/mongoc/event/encode/update", test_mongoc_event_update);

   run_test("/mongoc/event/decode/reply", test_mongoc_event_decode_reply);
   run_test("/mongoc/event/decode/msg", test_mongoc_event_decode_msg);

   return 0;
}
