#include <fcntl.h>
#include <mongoc.h>
#include <mongoc-buffer-private.h>

#include "mongoc-tests.h"


static void
test_mongoc_buffer_basic (void)
{
   bson_uint8_t lbuf[1024];
   mongoc_stream_t *stream;
   mongoc_buffer_t buf;
   bson_uint8_t *data = bson_malloc0(1024);
   bson_error_t error = { 0 };
   struct iovec iov;
   int fd;

   fd = open("tests/binary/reply1.dat", O_RDONLY);
   assert(fd >= 0);

   stream = mongoc_stream_new_from_unix(fd);
   assert(stream);

   mongoc_buffer_init(&buf, data, 1024, bson_realloc);
   assert(TRUE == mongoc_buffer_fill(&buf, stream, 536, &error));

   iov.iov_base = lbuf;
   iov.iov_len = 1024;
   assert_cmpint(536, ==, mongoc_buffer_readv(&buf, &iov, 1));
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/buffer/basic", test_mongoc_buffer_basic);

   return 0;
}
