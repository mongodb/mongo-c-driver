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


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/event/query", test_mongoc_event_query);

   return 0;
}
