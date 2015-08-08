#include <mongoc.h>
#define MONGOC_INSIDE
#include <mongoc-gridfs-file-private.h>
#undef MONGOC_INSIDE

#include "test-libmongoc.h"
#include "mongoc-tests.h"
#include "TestSuite.h"


static mongoc_gridfs_t *
get_test_gridfs (mongoc_client_t *client,
                 const char      *name,
                 bson_error_t    *error)
{
   char *gen;
   char n [48];

   gen = gen_collection_name ("fs");
   bson_snprintf (n, sizeof n, "%s_%s", gen, name);
   bson_free (gen);

   return mongoc_client_get_gridfs (client, "test", n, error);
}

bool
drop_collections (mongoc_gridfs_t *gridfs,
                  bson_error_t    *error)
{
   return (mongoc_collection_drop (mongoc_gridfs_get_files (gridfs), error) &&
           mongoc_collection_drop (mongoc_gridfs_get_chunks (gridfs), error));
}


static void
test_create (void)
{
   mongoc_gridfs_t *gridfs;
   mongoc_gridfs_file_t *file;
   mongoc_client_t *client;
   bson_error_t error;

   client = test_framework_client_new ();
   assert (client);

   ASSERT_OR_PRINT (
      (gridfs = mongoc_client_get_gridfs (client, "test", "foo", &error)),
      error);

   mongoc_gridfs_drop (gridfs, &error);

   file = mongoc_gridfs_create_file (gridfs, NULL);
   assert (file);
   assert (mongoc_gridfs_file_save (file));

   mongoc_gridfs_file_destroy (file);

   drop_collections (gridfs, &error);
   mongoc_gridfs_destroy (gridfs);

   mongoc_client_destroy (client);
}


static void
test_remove (void)
{
   mongoc_gridfs_t *gridfs;
   mongoc_gridfs_file_t *file;
   mongoc_gridfs_file_opt_t opts = { 0 };
   mongoc_client_t *client;
   bson_error_t error;
   char name[32];

   client = test_framework_client_new ();
   assert (client);

   ASSERT_OR_PRINT (gridfs = mongoc_client_get_gridfs (
      client, "test", "foo", &error), error);

   mongoc_gridfs_drop (gridfs, &error);

   bson_snprintf (name, sizeof name, "test-remove.%u", rand ());
   opts.filename = name;

   file = mongoc_gridfs_create_file (gridfs, &opts);
   assert (file);
   assert (mongoc_gridfs_file_save (file));

   ASSERT_OR_PRINT (mongoc_gridfs_file_remove (file, &error), error);

   mongoc_gridfs_file_destroy (file);

   file = mongoc_gridfs_find_one_by_filename (gridfs, name, &error);
   assert (!file);

   drop_collections (gridfs, &error);
   mongoc_gridfs_destroy (gridfs);

   mongoc_client_destroy (client);
}


static void
test_list (void)
{
   mongoc_gridfs_t *gridfs;
   mongoc_gridfs_file_t *file;
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_gridfs_file_list_t *list;
   mongoc_gridfs_file_opt_t opt = { 0 };
   bson_t query, child;
   char buf[100];
   int i = 0;

   client = test_framework_client_new ();
   assert (client);

   ASSERT_OR_PRINT (gridfs = get_test_gridfs (client, "list", &error), error);

   mongoc_gridfs_drop (gridfs, &error);

   for (i = 0; i < 3; i++) {
      bson_snprintf (buf, sizeof buf, "file.%d", i);
      opt.filename = buf;
      file = mongoc_gridfs_create_file (gridfs, &opt);
      assert (file);
      assert (mongoc_gridfs_file_save (file));
      mongoc_gridfs_file_destroy (file);
   }

   bson_init (&query);
   bson_append_document_begin (&query, "$orderby", -1, &child);
   bson_append_int32 (&child, "filename", -1, 1);
   bson_append_document_end (&query, &child);
   bson_append_document_begin (&query, "$query", -1, &child);
   bson_append_document_end (&query, &child);

   list = mongoc_gridfs_find (gridfs, &query);

   bson_destroy (&query);

   i = 0;
   while ((file = mongoc_gridfs_file_list_next (list))) {
      bson_snprintf (buf, sizeof buf, "file.%d", i++);

      assert (strcmp (mongoc_gridfs_file_get_filename (file), buf) == 0);

      mongoc_gridfs_file_destroy (file);
   }
   assert(i == 3);
   mongoc_gridfs_file_list_destroy (list);

   bson_init (&query);
   bson_append_utf8 (&query, "filename", -1, "file.1", -1);
   ASSERT_OR_PRINT (file = mongoc_gridfs_find_one (gridfs, &query, &error),
                    error);

   assert (strcmp (mongoc_gridfs_file_get_filename (file), "file.1") == 0);
   mongoc_gridfs_file_destroy (file);

   ASSERT_OR_PRINT (
      file = mongoc_gridfs_find_one_by_filename (gridfs, "file.1", &error),
      error);

   assert (strcmp (mongoc_gridfs_file_get_filename (file), "file.1") == 0);
   mongoc_gridfs_file_destroy (file);

   drop_collections (gridfs, &error);
   mongoc_gridfs_destroy (gridfs);

   mongoc_client_destroy (client);
}


static void
test_properties (void)
{
   mongoc_client_t *client;
   mongoc_gridfs_t *gridfs;
   bson_error_t error;
   bson_t *doc_in;
   mongoc_gridfs_file_t *file;
   mongoc_gridfs_file_list_t *list;
   bson_t query = BSON_INITIALIZER;
   const bson_value_t *file_id;
   const char *alias0, *alias1;

   client = test_framework_client_new ();

   ASSERT_OR_PRINT (gridfs = get_test_gridfs (client, "list", &error), error);

   mongoc_gridfs_drop (gridfs, &error);

   /* the C Driver sets _id to an ObjectId, but other drivers can do anything */
   doc_in = BCON_NEW (
         "_id", BCON_INT32 (1),
         "md5", BCON_UTF8 ("md5"),
         "filename", BCON_UTF8 ("filename"),
         "contentType", BCON_UTF8 ("content_type"),
         "aliases", "[", BCON_UTF8 ("alias0"), BCON_UTF8 ("alias1"), "]",
         "metadata", "{", "key", BCON_UTF8 ("value"), "}",
         "chunkSize", BCON_INT32 (100));

   assert (mongoc_collection_insert (mongoc_gridfs_get_files (gridfs),
                                     MONGOC_INSERT_NONE, doc_in, NULL, NULL));

   list = mongoc_gridfs_find (gridfs, &query);
   file = mongoc_gridfs_file_list_next (list);
   file_id = mongoc_gridfs_file_get_id (file);
   assert (file_id);
   ASSERT_CMPINT (BSON_TYPE_INT32, ==, file_id->value_type);
   ASSERT_CMPINT (1, ==, file_id->value.v_int32);
   ASSERT_CMPSTR ("md5", mongoc_gridfs_file_get_md5 (file));
   ASSERT_CMPSTR ("filename", mongoc_gridfs_file_get_filename (file));
   ASSERT_CMPSTR ("content_type", mongoc_gridfs_file_get_content_type (file));
   assert (BCON_EXTRACT ((bson_t *)mongoc_gridfs_file_get_aliases (file),
                         "0", BCONE_UTF8 (alias0),
                         "1", BCONE_UTF8 (alias1)));

   ASSERT_CMPSTR ("alias0", alias0);
   ASSERT_CMPSTR ("alias1", alias1);

   drop_collections (gridfs, &error);
   mongoc_gridfs_file_destroy (file);
   mongoc_gridfs_file_list_destroy (list);
   bson_destroy (doc_in);
   mongoc_gridfs_destroy (gridfs);
   mongoc_client_destroy (client);
}


static void
test_create_from_stream (void)
{
   mongoc_gridfs_t *gridfs;
   mongoc_gridfs_file_t *file;
   mongoc_stream_t *stream;
   mongoc_client_t *client;
   bson_error_t error;

   client = test_framework_client_new ();
   assert (client);

   ASSERT_OR_PRINT ((gridfs = get_test_gridfs (client, "from_stream", &error)),
                    error);

   mongoc_gridfs_drop (gridfs, &error);

   stream = mongoc_stream_file_new_for_path (BINARY_DIR"/gridfs.dat", O_RDONLY, 0);
   assert (stream);

   file = mongoc_gridfs_create_file_from_stream (gridfs, stream, NULL);
   assert (file);
   assert (mongoc_gridfs_file_save (file));

   mongoc_gridfs_file_destroy (file);

   drop_collections (gridfs, &error);
   mongoc_gridfs_destroy (gridfs);

   mongoc_client_destroy (client);
}


static void
test_read (void)
{
   mongoc_gridfs_t *gridfs;
   mongoc_gridfs_file_t *file;
   mongoc_stream_t *stream;
   mongoc_client_t *client;
   bson_error_t error;
   ssize_t r;
   char buf[10], buf2[10];
   mongoc_iovec_t iov[2];

   iov[0].iov_base = buf;
   iov[0].iov_len = 10;

   iov[1].iov_base = buf2;
   iov[1].iov_len = 10;

   client = test_framework_client_new ();
   assert (client);

   ASSERT_OR_PRINT (gridfs = get_test_gridfs (client, "read", &error), error);

   mongoc_gridfs_drop (gridfs, &error);

   stream = mongoc_stream_file_new_for_path (BINARY_DIR"/gridfs.dat", O_RDONLY, 0);

   file = mongoc_gridfs_create_file_from_stream (gridfs, stream, NULL);
   assert (file);
   assert (mongoc_gridfs_file_save (file));

   r = mongoc_gridfs_file_readv (file, iov, 2, 20, 0);
   assert (r == 20);
   assert (memcmp (iov[0].iov_base, "Bacon ipsu", 10) == 0);
   assert (memcmp (iov[1].iov_base, "m dolor si", 10) == 0);

   assert (mongoc_gridfs_file_seek (file, 0, SEEK_SET) == 0);
   r = mongoc_gridfs_file_readv (file, iov, 2, 20, 0);

   assert (r == 20);
   assert (memcmp (iov[0].iov_base, "Bacon ipsu", 10) == 0);
   assert (memcmp (iov[1].iov_base, "m dolor si", 10) == 0);

   mongoc_gridfs_file_destroy (file);

   drop_collections (gridfs, &error);
   mongoc_gridfs_destroy (gridfs);

   mongoc_client_destroy (client);
}


static void
test_write (void)
{
   mongoc_gridfs_t *gridfs;
   mongoc_gridfs_file_t *file;
   mongoc_client_t *client;
   bson_error_t error;
   ssize_t r;
   char buf[] = "foo bar";
   char buf2[] = " baz";
   char buf3[1000];
   mongoc_gridfs_file_opt_t opt = { 0 };
   mongoc_iovec_t iov[2];
   mongoc_iovec_t riov;
   int len = sizeof buf + sizeof buf2 - 2;

   iov [0].iov_base = buf;
   iov [0].iov_len = sizeof (buf) - 1;
   iov [1].iov_base = buf2;
   iov [1].iov_len = sizeof (buf2) - 1;

   riov.iov_base = buf3;
   riov.iov_len = sizeof (buf3);

   opt.chunk_size = 2;

   client = test_framework_client_new ();
   assert (client);

   ASSERT_OR_PRINT (gridfs = get_test_gridfs (client, "write", &error), error);

   mongoc_gridfs_drop (gridfs, &error);

   file = mongoc_gridfs_create_file (gridfs, &opt);
   assert (file);
   assert (mongoc_gridfs_file_save (file));

   r = mongoc_gridfs_file_writev (file, iov, 2, 0);
   assert (r == len);
   assert (mongoc_gridfs_file_save (file));

   r = mongoc_gridfs_file_seek (file, 0, SEEK_SET);
   assert (!r);

   r = (ssize_t) mongoc_gridfs_file_tell (file);
   assert (r == 0);

   r = mongoc_gridfs_file_readv (file, &riov, 1, len, 0);
   assert (r == len);
   assert (memcmp (buf3, "foo bar baz", len) == 0);

   mongoc_gridfs_file_destroy (file);

   drop_collections (gridfs, &error);
   mongoc_gridfs_destroy (gridfs);

   mongoc_client_destroy (client);
}


static void
test_stream (void)
{
   mongoc_gridfs_t *gridfs;
   mongoc_gridfs_file_t *file;
   mongoc_client_t *client;
   mongoc_stream_t *stream;
   mongoc_stream_t *in_stream;
   bson_error_t error;
   ssize_t r;
   char buf[4096];
   mongoc_iovec_t iov;

   iov.iov_base = buf;
   iov.iov_len = sizeof buf;

   client = test_framework_client_new ();
   assert (client);

   ASSERT_OR_PRINT (gridfs = get_test_gridfs (client, "fs", &error), error);

   mongoc_gridfs_drop (gridfs, &error);

   in_stream = mongoc_stream_file_new_for_path (BINARY_DIR"/gridfs.dat", O_RDONLY, 0);

   file = mongoc_gridfs_create_file_from_stream (gridfs, in_stream, NULL);
   assert (file);
   assert (mongoc_gridfs_file_save (file));

   stream = mongoc_stream_gridfs_new (file);

   r = mongoc_stream_readv (stream, &iov, 1, file->length, 0);
   assert (r == file->length);

   /* cleanup */
   mongoc_stream_destroy (stream);

   mongoc_gridfs_file_destroy (file);

   drop_collections (gridfs, &error);
   mongoc_gridfs_destroy (gridfs);
   mongoc_client_destroy (client);
}


static void
test_remove_by_filename (void)
{
   mongoc_gridfs_t *gridfs;
   mongoc_gridfs_file_t *file;
   mongoc_gridfs_file_opt_t opt = { 0 };
   mongoc_client_t *client;
   bson_error_t error;

   client = test_framework_client_new ();
   assert (client);

   ASSERT_OR_PRINT (gridfs = get_test_gridfs (
      client, "fs_remove_by_filename", &error), error);

   mongoc_gridfs_drop (gridfs, &error);

   opt.filename = "foo_file_1.txt";
   file = mongoc_gridfs_create_file (gridfs, &opt);
   assert (file);
   assert (mongoc_gridfs_file_save (file));
   mongoc_gridfs_file_destroy (file);

   opt.filename = "foo_file_2.txt";
   file = mongoc_gridfs_create_file (gridfs, &opt);
   assert (file);
   assert (mongoc_gridfs_file_save (file));

   ASSERT_OR_PRINT (
      mongoc_gridfs_remove_by_filename (gridfs, "foo_file_1.txt", &error),
      error);
   mongoc_gridfs_file_destroy (file);

   file = mongoc_gridfs_find_one_by_filename (gridfs, "foo_file_1.txt", &error);
   assert (!file);

   file = mongoc_gridfs_find_one_by_filename (gridfs, "foo_file_2.txt", &error);
   assert (file);
   mongoc_gridfs_file_destroy (file);

   drop_collections (gridfs, &error);
   mongoc_gridfs_destroy (gridfs);

   mongoc_client_destroy (client);
}

void
test_gridfs_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/GridFS/create", test_create);
   TestSuite_Add (suite, "/GridFS/create_from_stream", test_create_from_stream);
   TestSuite_Add (suite, "/GridFS/list", test_list);
   TestSuite_Add (suite, "/GridFS/properties", test_properties);
   TestSuite_Add (suite, "/GridFS/read", test_read);
   TestSuite_Add (suite, "/GridFS/stream", test_stream);
   TestSuite_Add (suite, "/GridFS/remove", test_remove);
   TestSuite_Add (suite, "/GridFS/write", test_write);
   TestSuite_Add (suite, "/GridFS/remove_by_filename", test_remove_by_filename);
}
