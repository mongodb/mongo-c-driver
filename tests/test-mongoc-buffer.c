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

   r = mongoc_buffer_fill(&buf, stream, 537, 0, &error);
   assert_cmpint(r, ==, -1);
   r = mongoc_buffer_fill(&buf, stream, 536, 0, &error);
   assert_cmpint(r, ==, 536);
   assert(buf.len == 536);

   mongoc_buffer_destroy(&buf);
   mongoc_buffer_destroy(&buf);
   mongoc_buffer_destroy(&buf);
   mongoc_buffer_destroy(&buf);

   mongoc_stream_destroy(stream);
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
main (int   argc,
      char *argv[])
{
   if (argc <= 1 || !!strcmp(argv[1], "-v")) {
      mongoc_log_set_handler(log_handler, NULL);
   }

   run_test("/mongoc/buffer/basic", test_mongoc_buffer_basic);

   return 0;
}
