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


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/event/delete", test_mongoc_event_delete);
   run_test("/mongoc/event/get_more", test_mongoc_event_get_more);
   run_test("/mongoc/event/insert", test_mongoc_event_insert);
   run_test("/mongoc/event/query", test_mongoc_event_query);
   run_test("/mongoc/event/query_no_fields", test_mongoc_event_query_no_fields);
   run_test("/mongoc/event/update", test_mongoc_event_update);

   return 0;
}
