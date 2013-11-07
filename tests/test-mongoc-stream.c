#include <fcntl.h>
#include <mongoc.h>
#include <stdlib.h>
#include <unistd.h>

#include "mongoc-tests.h"


static void
test_buffered_basic (void)
{
   mongoc_stream_t *stream;
   mongoc_stream_t *buffered;
   ssize_t r;
   struct iovec iov;
   char buf[16236];
   int fd;

   fd = open("tests/binary/reply2.dat", O_RDONLY);
   assert(fd != -1);

   /* stream assumes ownership of fd */
   stream = mongoc_stream_unix_new(fd);

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
   ssize_t r;
   struct iovec iov;
   char buf[16236];
   int fd;

   fd = open("tests/binary/reply2.dat", O_RDONLY);
   assert(fd != -1);

   /* stream assumes ownership of fd */
   stream = mongoc_stream_unix_new(fd);

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


static void
log_handler (mongoc_log_level_t  log_level,
             const char         *domain,
             const char         *message,
             void               *user_data)
{
   /* Do Nothing */
}


int
main (int argc,
      char *argv[])
{
   if (argc <= 1 || !!strcmp(argv[1], "-v")) {
      mongoc_log_set_handler(log_handler, NULL);
   }

   run_test("/mongoc/stream/buffered/basic", test_buffered_basic);
   run_test("/mongoc/stream/buffered/oversized", test_buffered_oversized);

   return 0;
}
