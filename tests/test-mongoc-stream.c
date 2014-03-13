#include "mongoc-tests.h"

#include <fcntl.h>
#include <mongoc.h>
#include <stdlib.h>

#include "TestSuite.h"


static void
test_buffered_basic (void)
{
   mongoc_stream_t *stream;
   mongoc_stream_t *buffered;
   mongoc_iovec_t iov;
   ssize_t r;
   char buf[16236];

   stream = mongoc_stream_file_new_for_path (BINARY_DIR"/reply2.dat", O_RDONLY, 0);
   assert (stream);

   /* buffered assumes ownership of stream */
   buffered = mongoc_stream_buffered_new(stream, 1024);

   /* try to read large chunk larger than buffer. */
   iov.iov_len = sizeof buf;
   iov.iov_base = buf;
   r = mongoc_stream_readv(buffered, &iov, 1, iov.iov_len, -1);
   BSON_ASSERT(r == iov.iov_len);

   /* cleanup */
   mongoc_stream_destroy(buffered);
}


static void
test_buffered_oversized (void)
{
   mongoc_stream_t *stream;
   mongoc_stream_t *buffered;
   mongoc_iovec_t iov;
   ssize_t r;
   char buf[16236];

   stream = mongoc_stream_file_new_for_path (BINARY_DIR"/reply2.dat", O_RDONLY, 0);
   assert (stream);

   /* buffered assumes ownership of stream */
   buffered = mongoc_stream_buffered_new(stream, 20000);

   /* try to read large chunk larger than buffer. */
   iov.iov_len = sizeof buf;
   iov.iov_base = buf;
   r = mongoc_stream_readv(buffered, &iov, 1, iov.iov_len, -1);
   BSON_ASSERT(r == iov.iov_len);

   /* cleanup */
   mongoc_stream_destroy(buffered);
}


void
test_stream_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Stream/buffered/basic", test_buffered_basic);
   TestSuite_Add (suite, "/Stream/buffered/oversized", test_buffered_oversized);
}
