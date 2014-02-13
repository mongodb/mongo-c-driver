#include <fcntl.h>
#include <mongoc.h>
#include <mongoc-buffer-private.h>

#include "TestSuite.h"


static void
test_mongoc_buffer_basic (void)
{
   mongoc_stream_t *stream;
   mongoc_buffer_t buf;
   uint8_t *data = bson_malloc0(1024);
   bson_error_t error = { 0 };
   ssize_t r;
   mongoc_fd_t fd;

   fd = mongoc_open("tests/binary/reply1.dat", O_RDONLY);
   ASSERT(mongoc_fd_is_valid(fd));

   stream = mongoc_stream_unix_new(fd);
   ASSERT(stream);

   _mongoc_buffer_init(&buf, data, 1024, bson_realloc);

   r = _mongoc_buffer_fill(&buf, stream, 537, 0, &error);
   ASSERT_CMPINT((int)r, ==, -1);
   r = _mongoc_buffer_fill(&buf, stream, 536, 0, &error);
   ASSERT_CMPINT((int)r, ==, 536);
   ASSERT(buf.len == 536);

   _mongoc_buffer_destroy(&buf);
   _mongoc_buffer_destroy(&buf);
   _mongoc_buffer_destroy(&buf);
   _mongoc_buffer_destroy(&buf);

   mongoc_stream_destroy(stream);
}


void
test_buffer_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Buffer/Basic", test_mongoc_buffer_basic);
}
