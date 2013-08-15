#include <fcntl.h>
#include <mongoc.h>
#include <mongoc-buffer-private.h>

#include "mongoc-tests.h"


static void
test_mongoc_buffer_basic (void)
{
   mongoc_stream_t *stream;
   mongoc_buffer_t buf;
   bson_uint8_t *data = bson_malloc0(1024);
   bson_error_t error = { 0 };
   bson_bool_t r;
   int fd;

   fd = open("tests/binary/reply1.dat", O_RDONLY);
   assert(fd >= 0);

   stream = mongoc_stream_unix_new(fd);
   assert(stream);

   mongoc_buffer_init(&buf, data, 1024, bson_realloc);

   r = mongoc_buffer_fill(&buf, stream, &error);
   assert_cmpint(r, ==, 536);
   assert_cmpint(buf.len, ==, 536);

   mongoc_buffer_destroy(&buf);
   mongoc_buffer_destroy(&buf);
   mongoc_buffer_destroy(&buf);
   mongoc_buffer_destroy(&buf);

   mongoc_stream_destroy(stream);
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/buffer/basic", test_mongoc_buffer_basic);

   return 0;
}
