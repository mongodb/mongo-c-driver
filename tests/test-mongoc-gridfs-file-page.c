#include <mongoc.h>
#include <mongoc-gridfs-file-page-private.h>

#include "mongoc-tests.h"

static void
test_create (void)
{
   bson_uint8_t fox[] = "the quick brown fox jumped over the laxy dog";
   bson_uint32_t len = sizeof fox;

   mongoc_gridfs_file_page_t *page;

   page = _mongoc_gridfs_file_page_new (fox, len, 4096);

   assert (page);

   _mongoc_gridfs_file_page_destroy (page);
}


static void
test_is_dirty (void)
{
   bson_uint8_t buf[] = "abcde";
   bson_uint32_t len = sizeof buf;
   bson_int32_t r;

   mongoc_gridfs_file_page_t *page;

   page = _mongoc_gridfs_file_page_new (buf, len, 10);
   assert (page);

   r = _mongoc_gridfs_file_page_is_dirty (page);
   assert (!r);

   r = _mongoc_gridfs_file_page_write (page, "foo", 3);
   assert (r == 3);

   r = _mongoc_gridfs_file_page_is_dirty (page);
   assert (r);

   _mongoc_gridfs_file_page_destroy (page);
}


static void
test_get_data (void)
{
   bson_uint8_t buf[] = "abcde";
   bson_uint32_t len = sizeof buf;
   const bson_uint8_t *ptr;
   bson_int32_t r;

   mongoc_gridfs_file_page_t *page;

   page = _mongoc_gridfs_file_page_new (buf, len, 10);
   assert (page);

   ptr = _mongoc_gridfs_file_page_get_data (page);
   assert (ptr == buf);

   r = _mongoc_gridfs_file_page_write (page, "foo", 3);
   assert (r == 3);

   ptr = _mongoc_gridfs_file_page_get_data (page);
   assert (ptr != buf);

   _mongoc_gridfs_file_page_destroy (page);
}


static void
test_get_len (void)
{
   bson_uint8_t buf[] = "abcde";
   bson_uint32_t len = sizeof buf;
   bson_int32_t r;

   mongoc_gridfs_file_page_t *page;

   page = _mongoc_gridfs_file_page_new (buf, len, 10);
   assert (page);

   r = _mongoc_gridfs_file_page_get_len (page);
   assert (r == 6);

   _mongoc_gridfs_file_page_destroy (page);
}


static void
test_read (void)
{
   bson_uint8_t fox[] = "the quick brown fox jumped over the laxy dog";
   bson_uint32_t len = sizeof fox;
   bson_int32_t r;
   char buf[100];

   mongoc_gridfs_file_page_t *page;

   page = _mongoc_gridfs_file_page_new (fox, len, 4096);

   assert (page);

   r = _mongoc_gridfs_file_page_read (page, buf, 3);
   assert (r == 3);
   assert (memcmp ("the", buf, 3) == 0);
   assert (page->offset == 3);

   r = _mongoc_gridfs_file_page_read (page, buf, 50);
   assert (r == len - 3);
   assert (memcmp (fox + 3, buf, len - 3) == 0);

   _mongoc_gridfs_file_page_destroy (page);
}


static void
test_seek (void)
{
   bson_uint8_t fox[] = "the quick brown fox jumped over the laxy dog";
   bson_uint32_t len = sizeof fox;
   bson_int32_t r;

   mongoc_gridfs_file_page_t *page;

   page = _mongoc_gridfs_file_page_new (fox, len, 4096);

   assert (page);

   r = _mongoc_gridfs_file_page_seek (page, 4);
   assert (r);
   assert (page->offset == 4);

   r = _mongoc_gridfs_file_page_tell (page);
   assert (r = 4);

   _mongoc_gridfs_file_page_destroy (page);
}


static void
test_write (void)
{
   bson_uint8_t buf[] = "abcde";
   bson_uint32_t len = sizeof buf;
   bson_int32_t r;

   mongoc_gridfs_file_page_t *page;

   page = _mongoc_gridfs_file_page_new (buf, len, 10);

   assert (page);
   assert (page->len == len);
   assert (!page->buf);

   r = _mongoc_gridfs_file_page_write (page, "1", 1);
   assert (r == 1);
   assert (page->buf);
   assert (memcmp (page->buf, "1bcde", len) == 0);
   assert (page->offset == 1);
   assert (page->len == len);

   r = _mongoc_gridfs_file_page_write (page, "234567", 6);
   assert (r == 6);
   assert (memcmp (page->buf, "1234567", 7) == 0);
   assert (page->offset == 7);
   assert (page->len == 7);

   r = _mongoc_gridfs_file_page_write (page, "8910", 4);
   assert (r == 3);
   assert (memcmp (page->buf, "1234567891", 10) == 0);
   assert (page->offset == 10);
   assert (page->len == 10);

   r = _mongoc_gridfs_file_page_write (page, "foo", 3);
   assert (r == 0);

   _mongoc_gridfs_file_page_destroy (page);
}


static void
log_handler (mongoc_log_level_t log_level,
             const char        *domain,
             const char        *message,
             void              *user_data)
{
   /* Do Nothing */
}


int
main (int   argc,
      char *argv[])
{
   if (argc <= 1 || !!strcmp (argv[1], "-v")) {
      mongoc_log_set_handler (log_handler, NULL);
   }

   run_test ("/mongoc/gridfs/file/page/create", test_create);
   run_test ("/mongoc/gridfs/file/page/get_data", test_get_data);
   run_test ("/mongoc/gridfs/file/page/get_len", test_get_len);
   run_test ("/mongoc/gridfs/file/page/is_dirty", test_is_dirty);
   run_test ("/mongoc/gridfs/file/page/read", test_read);
   run_test ("/mongoc/gridfs/file/page/seek", test_seek);
   run_test ("/mongoc/gridfs/file/page/write", test_write);

   return 0;
}
