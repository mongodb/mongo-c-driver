#include <bcon.h>
#include <mongoc.h>
#include <mongoc-client-private.h>

#include "TestSuite.h"

#include "test-libmongoc.h"
#include "mongoc-tests.h"

static char *gTestUri;


static mongoc_database_t *
get_test_database (mongoc_client_t *client)
{
   return mongoc_client_get_database (client, "test");
}


static mongoc_collection_t *
get_test_collection (mongoc_client_t *client,
                     const char      *prefix)
{
   mongoc_collection_t *ret;
   char *str;

   str = gen_collection_name (prefix);
   ret = mongoc_client_get_collection (client, "test", str);
   bson_free (str);

   return ret;
}


static void
test_insert (void)
{
   mongoc_database_t *database;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_context_t *context;
   bson_error_t error;
   bool r;
   bson_oid_t oid;
   unsigned i;
   bson_t b;


   client = mongoc_client_new(gTestUri);
   ASSERT (client);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_insert");
   ASSERT (collection);

   mongoc_collection_drop(collection, &error);

   context = bson_context_new(BSON_CONTEXT_NONE);
   ASSERT (context);

   for (i = 0; i < 10; i++) {
      bson_init(&b);
      bson_oid_init(&oid, context);
      bson_append_oid(&b, "_id", 3, &oid);
      bson_append_utf8(&b, "hello", 5, "/world", 5);
      r = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &b, NULL,
                                   &error);
      if (!r) {
         MONGOC_WARNING("%s\n", error.message);
      }
      ASSERT (r);
      bson_destroy(&b);
   }

   bson_init (&b);
   BSON_APPEND_INT32 (&b, "$hello", 1);
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, &b, NULL,
                                 &error);
   ASSERT (!r);
   ASSERT (error.domain == MONGOC_ERROR_BSON);
   ASSERT (error.code == MONGOC_ERROR_BSON_INVALID);
   bson_destroy (&b);

   r = mongoc_collection_drop (collection, &error);
   ASSERT (r);

   mongoc_collection_destroy(collection);
   mongoc_database_destroy(database);
   bson_context_destroy(context);
   mongoc_client_destroy(client);
}


static void
test_insert_bulk (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   bson_context_t *context;
   bson_error_t error;
   bool r;
   bson_oid_t oid;
   unsigned i;
   bson_t q;
   bson_t b[10];
   bson_t *bptr[10];
   int64_t count;

   client = mongoc_client_new(gTestUri);
   ASSERT (client);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_insert_bulk");
   ASSERT (collection);

   mongoc_collection_drop(collection, &error);

   context = bson_context_new(BSON_CONTEXT_NONE);
   ASSERT (context);

   bson_init(&q);
   bson_append_int32(&q, "n", -1, 0);

   for (i = 0; i < 10; i++) {
      bson_init(&b[i]);
      bson_oid_init(&oid, context);
      bson_append_oid(&b[i], "_id", -1, &oid);
      bson_append_int32(&b[i], "n", -1, i % 2);
      bptr[i] = &b[i];
   }

   BEGIN_IGNORE_DEPRECATIONS;
   r = mongoc_collection_insert_bulk (collection, MONGOC_INSERT_NONE,
                                      (const bson_t **)bptr, 10, NULL, &error);
   END_IGNORE_DEPRECATIONS;

   if (!r) {
      MONGOC_WARNING("%s\n", error.message);
   }
   ASSERT (r);

   count = mongoc_collection_count (collection, MONGOC_QUERY_NONE, &q, 0, 0, NULL, &error);
   ASSERT (count == 5);

   for (i = 8; i < 10; i++) {
      bson_destroy(&b[i]);
      bson_init(&b[i]);
      bson_oid_init(&oid, context);
      bson_append_oid(&b[i], "_id", -1, &oid);
      bson_append_int32(&b[i], "n", -1, i % 2);
      bptr[i] = &b[i];
   }

   BEGIN_IGNORE_DEPRECATIONS;
   r = mongoc_collection_insert_bulk (collection, MONGOC_INSERT_NONE,
                                      (const bson_t **)bptr, 10, NULL, &error);
   END_IGNORE_DEPRECATIONS;

   ASSERT (!r);
   ASSERT (error.code == 11000);

   count = mongoc_collection_count (collection, MONGOC_QUERY_NONE, &q, 0, 0, NULL, &error);

   /*
    * MongoDB <2.6 and 2.6 will return different values for this. This is a
    * primary reason that mongoc_collection_insert_bulk() is deprecated.
    * Instead, you should use the new bulk api which will hide the differences
    * for you.  However, since the new bulk API is slower on 2.4 when write
    * concern is needed for inserts, we will support this for a while, albeit
    * deprecated.
    */
   if (client->cluster.nodes [0].max_wire_version == 0) {
      ASSERT (count == 6);
   } else {
      ASSERT (count == 5);
   }

   BEGIN_IGNORE_DEPRECATIONS;
   r = mongoc_collection_insert_bulk (collection, MONGOC_INSERT_CONTINUE_ON_ERROR,
                                      (const bson_t **)bptr, 10, NULL, &error);
   END_IGNORE_DEPRECATIONS;
   ASSERT (!r);
   ASSERT (error.code == 11000);

   count = mongoc_collection_count (collection, MONGOC_QUERY_NONE, &q, 0, 0, NULL, &error);
   ASSERT (count == 6);

   /* test validate */
   for (i = 0; i < 10; i++) {
      bson_destroy (&b[i]);
      bson_init (&b[i]);
      BSON_APPEND_INT32 (&b[i], "$invalid_dollar_prefixed_name", i);
      bptr[i] = &b[i];
   }
   BEGIN_IGNORE_DEPRECATIONS;
   r = mongoc_collection_insert_bulk (collection, MONGOC_INSERT_NONE,
                                      (const bson_t **)bptr, 10, NULL, &error);
   END_IGNORE_DEPRECATIONS;
   ASSERT (!r);
   ASSERT (error.domain == MONGOC_ERROR_BSON);
   ASSERT (error.code == MONGOC_ERROR_BSON_INVALID);

   bson_destroy(&q);
   for (i = 0; i < 10; i++) {
      bson_destroy(&b[i]);
   }

   r = mongoc_collection_drop (collection, &error);
   ASSERT (r);

   mongoc_collection_destroy(collection);
   mongoc_database_destroy(database);
   bson_context_destroy(context);
   mongoc_client_destroy(client);
}


static void
test_save (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   bson_context_t *context;
   bson_error_t error;
   bool r;
   bson_oid_t oid;
   unsigned i;
   bson_t b;

   client = mongoc_client_new(gTestUri);
   ASSERT (client);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_save");
   ASSERT (collection);

   mongoc_collection_drop (collection, &error);

   context = bson_context_new(BSON_CONTEXT_NONE);
   ASSERT (context);

   for (i = 0; i < 10; i++) {
      bson_init(&b);
      bson_oid_init(&oid, context);
      bson_append_oid(&b, "_id", 3, &oid);
      bson_append_utf8(&b, "hello", 5, "/world", 5);
      r = mongoc_collection_save(collection, &b, NULL, &error);
      if (!r) {
         MONGOC_WARNING("%s\n", error.message);
      }
      ASSERT (r);
      bson_destroy(&b);
   }

   bson_destroy (&b);

   r = mongoc_collection_drop (collection, &error);
   ASSERT (r);

   mongoc_collection_destroy(collection);
   mongoc_database_destroy(database);
   bson_context_destroy(context);
   mongoc_client_destroy(client);
}


static void
test_regex (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_write_concern_t *wr;
   mongoc_client_t *client;
   bson_error_t error = { 0 };
   int64_t count;
   bson_t q = BSON_INITIALIZER;
   bson_t *doc;
   bool r;

   client = mongoc_client_new (gTestUri);
   ASSERT (client);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_regex");
   ASSERT (collection);

   wr = mongoc_write_concern_new ();
   mongoc_write_concern_set_journal (wr, true);

   doc = BCON_NEW ("hello", "/world");
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, doc, wr, &error);
   ASSERT (r);

   BSON_APPEND_REGEX (&q, "hello", "^/wo", "i");

   count = mongoc_collection_count (collection,
                                    MONGOC_QUERY_NONE,
                                    &q,
                                    0,
                                    0,
                                    NULL,
                                    &error);

   ASSERT (count > 0);
   ASSERT (!error.domain);

   r = mongoc_collection_drop (collection, &error);
   ASSERT (r);

   mongoc_write_concern_destroy (wr);

   bson_destroy (&q);
   bson_destroy (doc);
   mongoc_collection_destroy (collection);
   mongoc_database_destroy(database);
   mongoc_client_destroy (client);
}


static void
test_update (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   bson_context_t *context;
   bson_error_t error;
   bool r;
   bson_oid_t oid;
   unsigned i;
   bson_t b;
   bson_t q;
   bson_t u;
   bson_t set;

   client = mongoc_client_new(gTestUri);
   ASSERT (client);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_update");
   ASSERT (collection);

   context = bson_context_new(BSON_CONTEXT_NONE);
   ASSERT (context);

   for (i = 0; i < 10; i++) {
      bson_init(&b);
      bson_oid_init(&oid, context);
      bson_append_oid(&b, "_id", 3, &oid);
      bson_append_utf8(&b, "utf8", 4, "utf8 string", 11);
      bson_append_int32(&b, "int32", 5, 1234);
      bson_append_int64(&b, "int64", 5, 12345678);
      bson_append_bool(&b, "bool", 4, 1);

      r = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &b, NULL, &error);
      if (!r) {
         MONGOC_WARNING("%s\n", error.message);
      }
      ASSERT (r);

      bson_init(&q);
      bson_append_oid(&q, "_id", 3, &oid);

      bson_init(&u);
      bson_append_document_begin(&u, "$set", 4, &set);
      bson_append_utf8(&set, "utf8", 4, "updated", 7);
      bson_append_document_end(&u, &set);

      r = mongoc_collection_update(collection, MONGOC_UPDATE_NONE, &q, &u, NULL, &error);
      if (!r) {
         MONGOC_WARNING("%s\n", error.message);
      }
      ASSERT (r);

      bson_destroy(&b);
      bson_destroy(&q);
      bson_destroy(&u);
   }

   bson_init(&q);
   bson_init(&u);
   BSON_APPEND_INT32 (&u, "abcd", 1);
   BSON_APPEND_INT32 (&u, "$hi", 1);
   r = mongoc_collection_update(collection, MONGOC_UPDATE_NONE, &q, &u, NULL, &error);
   ASSERT (!r);
   ASSERT (error.domain == MONGOC_ERROR_BSON);
   ASSERT (error.code == MONGOC_ERROR_BSON_INVALID);
   bson_destroy(&q);
   bson_destroy(&u);

   bson_init(&q);
   bson_init(&u);
   BSON_APPEND_INT32 (&u, "a.b.c.d", 1);
   r = mongoc_collection_update(collection, MONGOC_UPDATE_NONE, &q, &u, NULL, &error);
   ASSERT (!r);
   ASSERT (error.domain == MONGOC_ERROR_BSON);
   ASSERT (error.code == MONGOC_ERROR_BSON_INVALID);
   bson_destroy(&q);
   bson_destroy(&u);

   r = mongoc_collection_drop (collection, &error);
   ASSERT (r);

   mongoc_collection_destroy(collection);
   mongoc_database_destroy(database);
   bson_context_destroy(context);
   mongoc_client_destroy(client);
}


static void
test_remove (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   bson_context_t *context;
   bson_error_t error;
   bool r;
   bson_oid_t oid;
   bson_t b;
   int i;

   client = mongoc_client_new(gTestUri);
   ASSERT (client);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_remove");
   ASSERT (collection);

   context = bson_context_new(BSON_CONTEXT_NONE);
   ASSERT (context);

   for (i = 0; i < 100; i++) {
      bson_init(&b);
      bson_oid_init(&oid, context);
      bson_append_oid(&b, "_id", 3, &oid);
      bson_append_utf8(&b, "hello", 5, "world", 5);
      r = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &b, NULL,
                                   &error);
      if (!r) {
         MONGOC_WARNING("%s\n", error.message);
      }
      ASSERT (r);
      bson_destroy(&b);

      bson_init(&b);
      bson_append_oid(&b, "_id", 3, &oid);
      r = mongoc_collection_remove(collection, MONGOC_REMOVE_NONE, &b, NULL,
                                   &error);
      if (!r) {
         MONGOC_WARNING("%s\n", error.message);
      }
      ASSERT (r);
      bson_destroy(&b);
   }

   r = mongoc_collection_drop (collection, &error);
   ASSERT (r);

   mongoc_collection_destroy(collection);
   mongoc_database_destroy(database);
   bson_context_destroy(context);
   mongoc_client_destroy(client);
}

static void
test_index (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   mongoc_index_opt_t opt;
   bson_error_t error;
   bool r;
   bson_t keys;

   mongoc_index_opt_init(&opt);

   client = mongoc_client_new(gTestUri);
   ASSERT (client);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_index");
   ASSERT (collection);

   bson_init(&keys);
   bson_append_int32(&keys, "hello", -1, 1);
   r = mongoc_collection_create_index(collection, &keys, &opt, &error);
   ASSERT (r);

   r = mongoc_collection_create_index(collection, &keys, &opt, &error);
   ASSERT (r);

   r = mongoc_collection_drop_index(collection, "hello_1", &error);
   ASSERT (r);

   bson_destroy(&keys);

   r = mongoc_collection_drop (collection, &error);
   ASSERT (r);

   mongoc_collection_destroy(collection);
   mongoc_database_destroy(database);
   mongoc_client_destroy(client);
}

static void
test_index_compound (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   mongoc_index_opt_t opt;
   bson_error_t error;
   bool r;
   bson_t keys;

   mongoc_index_opt_init(&opt);

   client = mongoc_client_new(gTestUri);
   ASSERT (client);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_index_compound");
   ASSERT (collection);

   bson_init(&keys);
   bson_append_int32(&keys, "hello", -1, 1);
   bson_append_int32(&keys, "world", -1, -1);
   r = mongoc_collection_create_index(collection, &keys, &opt, &error);
   ASSERT (r);

   r = mongoc_collection_create_index(collection, &keys, &opt, &error);
   ASSERT (r);

   r = mongoc_collection_drop_index(collection, "hello_1_world_-1", &error);
   ASSERT (r);

   bson_destroy(&keys);

   r = mongoc_collection_drop (collection, &error);
   ASSERT (r);

   mongoc_collection_destroy(collection);
   mongoc_database_destroy(database);
   mongoc_client_destroy(client);
}

static void
test_index_geo (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   mongoc_index_opt_t opt;
   mongoc_index_opt_geo_t geo_opt;
   bson_error_t error;
   bool r;
   bson_t keys;

   mongoc_index_opt_init(&opt);
   mongoc_index_opt_geo_init(&geo_opt);

   client = mongoc_client_new(gTestUri);
   ASSERT (client);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_geo_index");
   ASSERT (collection);

   /* Create a basic 2d index */
   bson_init(&keys);
   BSON_APPEND_UTF8(&keys, "location", "2d");
   r = mongoc_collection_create_index(collection, &keys, &opt, &error);
   ASSERT (r);

   r = mongoc_collection_drop_index(collection, "location_2d", &error);
   ASSERT (r);

   /* Create a 2d index with bells and whistles */
   bson_init(&keys);
   BSON_APPEND_UTF8(&keys, "location", "2d");

   geo_opt.twod_location_min = -123;
   geo_opt.twod_location_max = +123;
   geo_opt.twod_bits_precision = 30;
   opt.geo_options = &geo_opt;

   if (client->cluster.nodes [0].max_wire_version > 0) {
      r = mongoc_collection_create_index(collection, &keys, &opt, &error);
         ASSERT (r);

      r = mongoc_collection_drop_index(collection, "location_2d", &error);
      ASSERT (r);
   }

   /* Create a Haystack index */
   bson_init(&keys);
   BSON_APPEND_UTF8(&keys, "location", "geoHaystack");
   BSON_APPEND_INT32(&keys, "category", 1);

   mongoc_index_opt_geo_init(&geo_opt);
   geo_opt.haystack_bucket_size = 5;
   opt.geo_options = &geo_opt;

   if (client->cluster.nodes [0].max_wire_version > 0) {
      r = mongoc_collection_create_index(collection, &keys, &opt, &error);
      ASSERT (r);

      r = mongoc_collection_drop_index(collection, "location_geoHaystack_category_1", &error);
      ASSERT (r);
   }

   mongoc_collection_destroy(collection);
   mongoc_database_destroy(database);
   mongoc_client_destroy(client);
}

static char *
storage_engine (mongoc_client_t *client)
{
   bson_iter_t iter;
   bson_error_t error;
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply;
   bool r;

   /* NOTE: this default will change eventually */
   char *engine = bson_strdup("mmapv1");

   BSON_APPEND_INT32 (&cmd, "getCmdLineOpts", 1);
   r = mongoc_client_command_simple(client, "admin", &cmd, NULL, &reply, &error);
   ASSERT (r);

   if (bson_iter_init_find (&iter, &reply, "parsed.storage.engine")) {
      engine = bson_strdup(bson_iter_utf8(&iter, NULL));
   }

   bson_destroy (&reply);
   bson_destroy (&cmd);

   return engine;
}

static void
test_index_storage (void)
{
   mongoc_collection_t *collection = NULL;
   mongoc_database_t *database = NULL;
   mongoc_client_t *client = NULL;
   mongoc_index_opt_t opt;
   mongoc_index_opt_wt_t wt_opt;
   bson_error_t error;
   bool r;
   bson_t keys;
   char *engine = NULL;

   client = mongoc_client_new (gTestUri);
   ASSERT (client);

   /* Skip unless we are on WT */
   engine = storage_engine(client);
   if (strcmp("wiredTiger", engine) != 0) {
      goto cleanup;
   }

   mongoc_index_opt_init (&opt);
   mongoc_index_opt_wt_init (&wt_opt);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_storage_index");
   ASSERT (collection);

   /* Create a simple index */
   bson_init (&keys);
   bson_append_int32 (&keys, "hello", -1, 1);

   /* Add storage option to the index */
   wt_opt.base.type = MONGOC_INDEX_STORAGE_OPT_WIREDTIGER;
   wt_opt.config_str = "block_compressor=zlib";

   opt.storage_options = (mongoc_index_opt_storage_t *)&wt_opt;

   r = mongoc_collection_create_index (collection, &keys, &opt, &error);
   ASSERT (r);

 cleanup:
   if (engine) bson_free (engine);
   if (collection) mongoc_collection_destroy (collection);
   if (database) mongoc_database_destroy (database);
   if (client) mongoc_client_destroy (client);
}

static void
test_count (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error;
   int64_t count;
   bson_t b;

   client = mongoc_client_new(gTestUri);
   ASSERT (client);

   collection = mongoc_client_get_collection(client, "test", "test");
   ASSERT (collection);

   bson_init(&b);
   count = mongoc_collection_count(collection, MONGOC_QUERY_NONE, &b,
                                   0, 0, NULL, &error);
   bson_destroy(&b);

   if (count == -1) {
      MONGOC_WARNING("%s\n", error.message);
   }
   ASSERT (count != -1);

   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
}


static void
test_count_with_opts (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error;
   int64_t count;
   bson_t b;
   bson_t opts;

   client = mongoc_client_new (gTestUri);
   ASSERT (client);

   collection = mongoc_client_get_collection (client, "test", "test");
   ASSERT (collection);

   bson_init (&opts);

   BSON_APPEND_UTF8 (&opts, "hint", "_id_");

   bson_init (&b);
   count = mongoc_collection_count_with_opts (collection, MONGOC_QUERY_NONE, &b,
                                              0, 0, &opts, NULL, &error);
   bson_destroy (&b);
   bson_destroy (&opts);

   if (count == -1) {
      MONGOC_WARNING ("%s\n", error.message);
   }

   ASSERT (count != -1);

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}


static void
test_drop (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   bson_error_t error;
   bson_t *doc;
   bool r;

   client = mongoc_client_new(gTestUri);
   ASSERT (client);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_drop");
   ASSERT (collection);

   doc = BCON_NEW("hello", "world");
   r = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, doc, NULL, &error);
   bson_destroy (doc);
   ASSERT (r);

   r = mongoc_collection_drop(collection, &error);
   ASSERT (r == true);

   r = mongoc_collection_drop(collection, &error);
   ASSERT (r == false);

   mongoc_collection_destroy(collection);
   mongoc_database_destroy(database);
   mongoc_client_destroy(client);
}


static void
test_aggregate (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_error_t error;
   bool did_alternate = false;
   bool r;
   bson_t opts;
   bson_t *pipeline;
   bson_t *b;
   bson_iter_t iter;
   int i;

   client = mongoc_client_new(gTestUri);
   ASSERT (client);

   database = get_test_database (client);
   ASSERT (database);

   collection = get_test_collection (client, "test_aggregate");
   ASSERT (collection);

   pipeline = BCON_NEW ("pipeline", "[", "{", "$match", "{", "hello", BCON_UTF8 ("world"), "}", "}", "]");
   b = BCON_NEW ("hello", BCON_UTF8 ("world"));

again:
   mongoc_collection_drop(collection, &error);

   for (i = 0; i < 2; i++) {
      r = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, b, NULL, &error);
      ASSERT (r);
   }

   for (i = 0; i < 2; i++) {
      if (i % 2 == 0) {
         cursor = mongoc_collection_aggregate(collection, MONGOC_QUERY_NONE, pipeline, NULL, NULL);
         ASSERT (cursor);
      } else {
         bson_init (&opts);
         BSON_APPEND_INT32 (&opts, "batchSize", 10);
         BSON_APPEND_BOOL (&opts, "allowDiskUse", true);

         cursor = mongoc_collection_aggregate(collection, MONGOC_QUERY_NONE, pipeline, &opts, NULL);
         ASSERT (cursor);

         bson_destroy (&opts);
      }

      for (i = 0; i < 2; i++) {
         /*
          * This can fail if we are connecting to a 2.0 MongoDB instance.
          */
         r = mongoc_cursor_next(cursor, &doc);
         if (mongoc_cursor_error(cursor, &error)) {
            if ((error.domain == MONGOC_ERROR_QUERY) &&
                (error.code == MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND)) {
               mongoc_cursor_destroy (cursor);
               break;
            }
            MONGOC_WARNING("[%d.%d] %s", error.domain, error.code, error.message);
         }

         ASSERT (r);
         ASSERT (doc);

         ASSERT (bson_iter_init_find (&iter, doc, "hello") &&
                 BSON_ITER_HOLDS_UTF8 (&iter));
      }

      r = mongoc_cursor_next(cursor, &doc);
      if (mongoc_cursor_error(cursor, &error)) {
         MONGOC_WARNING("%s", error.message);
      }
      ASSERT (!r);
      ASSERT (!doc);

      mongoc_cursor_destroy(cursor);
   }

   if (!did_alternate) {
      did_alternate = true;
      bson_destroy (pipeline);
      pipeline = BCON_NEW ("0", "{", "$match", "{", "hello", BCON_UTF8 ("world"), "}", "}");
      goto again;
   }

   r = mongoc_collection_drop(collection, &error);
   ASSERT (r);

   mongoc_collection_destroy(collection);
   mongoc_database_destroy(database);
   mongoc_client_destroy(client);
   bson_destroy(b);
   bson_destroy(pipeline);
}


static void
test_validate (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_iter_t iter;
   bson_error_t error;
   bson_t doc = BSON_INITIALIZER;
   bson_t opts = BSON_INITIALIZER;
   bson_t reply;
   bool r;

   client = mongoc_client_new (gTestUri);
   ASSERT (client);

   collection = get_test_collection (client, "test_validate");
   ASSERT (collection);

   r = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &doc, NULL, &error);
   assert (r);

   BSON_APPEND_BOOL (&opts, "full", true);

   r = mongoc_collection_validate (collection, &opts, &reply, &error);
   assert (r);

   assert (bson_iter_init_find (&iter, &reply, "ns"));
   assert (bson_iter_init_find (&iter, &reply, "valid"));

   bson_destroy (&reply);

   bson_reinit (&opts);
   BSON_APPEND_UTF8 (&opts, "full", "bad_value");

   r = mongoc_collection_validate (collection, &opts, &reply, &error);
   assert (!r);
   assert (error.domain == MONGOC_ERROR_BSON);
   assert (error.code == MONGOC_ERROR_BSON_INVALID);

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (&doc);
   bson_destroy (&opts);
}


static void
test_rename (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error;
   bson_t doc = BSON_INITIALIZER;
   bool r;

   client = mongoc_client_new (gTestUri);
   ASSERT (client);

   collection = get_test_collection (client, "test_rename");
   ASSERT (collection);

   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, &doc, NULL, &error);
   assert (r);

   r = mongoc_collection_rename (collection, "test", "test_rename_2", false, &error);
   assert (r);

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (&doc);
}


static void
test_stats (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error;
   bson_iter_t iter;
   bson_t stats;
   bson_t doc = BSON_INITIALIZER;
   bool r;

   client = mongoc_client_new (gTestUri);
   ASSERT (client);

   collection = get_test_collection (client, "test_stats");
   ASSERT (collection);

   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, &doc, NULL, &error);
   assert (r);

   r = mongoc_collection_stats (collection, NULL, &stats, &error);
   assert (r);

   assert (bson_iter_init_find (&iter, &stats, "ns"));

   assert (bson_iter_init_find (&iter, &stats, "count"));
   assert (bson_iter_as_int64 (&iter) >= 1);

   bson_destroy (&stats);

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (&doc);
}


static void
test_find_and_modify (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error;
   bson_iter_t iter;
   bson_iter_t citer;
   bson_t *update;
   bson_t doc = BSON_INITIALIZER;
   bson_t reply;
   bool r;

   client = mongoc_client_new (gTestUri);
   ASSERT (client);

   collection = get_test_collection (client, "test_find_and_modify");
   ASSERT (collection);

   BSON_APPEND_INT32 (&doc, "superduper", 77889);

   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, &doc, NULL, &error);
   assert (r);

   update = BCON_NEW ("$set", "{",
                         "superduper", BCON_INT32 (1234),
                      "}");

   r = mongoc_collection_find_and_modify (collection,
                                          &doc,
                                          NULL,
                                          update,
                                          NULL,
                                          false,
                                          false,
                                          true,
                                          &reply,
                                          &error);
   assert (r);

   assert (bson_iter_init_find (&iter, &reply, "value"));
   assert (BSON_ITER_HOLDS_DOCUMENT (&iter));
   assert (bson_iter_recurse (&iter, &citer));
   assert (bson_iter_find (&citer, "superduper"));
   assert (BSON_ITER_HOLDS_INT32 (&citer));
   assert (bson_iter_int32 (&citer) == 1234);

   assert (bson_iter_init_find (&iter, &reply, "lastErrorObject"));
   assert (BSON_ITER_HOLDS_DOCUMENT (&iter));
   assert (bson_iter_recurse (&iter, &citer));
   assert (bson_iter_find (&citer, "updatedExisting"));
   assert (BSON_ITER_HOLDS_BOOL (&citer));
   assert (bson_iter_bool (&citer));

   bson_destroy (&reply);
   bson_destroy (update);

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (&doc);
}


static void
test_large_return (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   bson_error_t error;
   const bson_t *doc = NULL;
   bson_oid_t oid;
   bson_t insert_doc = BSON_INITIALIZER;
   bson_t query = BSON_INITIALIZER;
   size_t len;
   char *str;
   bool r;

   client = mongoc_client_new (gTestUri);
   ASSERT (client);

   collection = get_test_collection (client, "test_large_return");
   ASSERT (collection);

   len = 1024 * 1024 * 4;
   str = bson_malloc (len);
   memset (str, (int)' ', len);
   str [len - 1] = '\0';

   bson_oid_init (&oid, NULL);
   BSON_APPEND_OID (&insert_doc, "_id", &oid);
   BSON_APPEND_UTF8 (&insert_doc, "big", str);

   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, &insert_doc, NULL, &error);
   assert (r);

   bson_destroy (&insert_doc);

   BSON_APPEND_OID (&query, "_id", &oid);

   cursor = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0, &query, NULL, NULL);
   assert (cursor);
   bson_destroy (&query);

   r = mongoc_cursor_next (cursor, &doc);
   assert (r);
   assert (doc);

   r = mongoc_cursor_next (cursor, &doc);
   assert (!r);

   mongoc_cursor_destroy (cursor);

   r = mongoc_collection_drop (collection, &error);
   if (!r) fprintf (stderr, "ERROR: %s\n", error.message);
   assert (r);

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_free (str);
}


static void
test_many_return (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   bson_error_t error;
   const bson_t *doc = NULL;
   bson_oid_t oid;
   bson_t query = BSON_INITIALIZER;
   bson_t **docs;
   bool r;
   int i;

   client = mongoc_client_new (gTestUri);
   ASSERT (client);

   collection = get_test_collection (client, "test_many_return");
   ASSERT (collection);

   docs = bson_malloc (sizeof(bson_t*) * 5000);

   for (i = 0; i < 5000; i++) {
      docs [i] = bson_new ();
      bson_oid_init (&oid, NULL);
      BSON_APPEND_OID (docs [i], "_id", &oid);
   }

BEGIN_IGNORE_DEPRECATIONS;

   r = mongoc_collection_insert_bulk (collection, MONGOC_INSERT_NONE, (const bson_t **)docs, 5000, NULL, &error);

END_IGNORE_DEPRECATIONS;

   assert (r);

   for (i = 0; i < 5000; i++) {
      bson_destroy (docs [i]);
   }

   bson_free (docs);

   cursor = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 6000, &query, NULL, NULL);
   assert (cursor);
   bson_destroy (&query);

   i = 0;

   while (mongoc_cursor_next (cursor, &doc)) {
      assert (doc);
      i++;
   }

   assert (i == 5000);

   r = mongoc_cursor_next (cursor, &doc);
   assert (!r);

   mongoc_cursor_destroy (cursor);

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}


static void
test_command_fq (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   const bson_t *doc = NULL;
   bson_t *cmd;
   bool r;

   client = mongoc_client_new (gTestUri);
   ASSERT (client);

   collection = get_test_collection (client, "$cmd.sys.inprog");
   ASSERT (collection);

   cmd = BCON_NEW ("query", "{", "}");

   cursor = mongoc_collection_command (collection, MONGOC_QUERY_NONE, 0, 1, 0, cmd, NULL, NULL);
   r = mongoc_cursor_next (cursor, &doc);
   assert (r);

   r = mongoc_cursor_next (cursor, &doc);
   assert (!r);

   mongoc_cursor_destroy (cursor);
   bson_destroy (cmd);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}

static void
test_get_index_info (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   mongoc_index_opt_t opt1;
   mongoc_index_opt_t opt2;
   bson_error_t error = { 0 };
   mongoc_cursor_t *cursor;
   const bson_t *indexinfo;
   bson_t indexkey1;
   bson_t indexkey2;
   bson_t dummy = BSON_INITIALIZER;
   bson_iter_t idx_spec_iter;
   bson_iter_t idx_spec_iter_copy;
   bool r;
   const char *cur_idx_name;
   char *idx1_name = NULL;
   char *idx2_name = NULL;
   const char *id_idx_name = "_id_";
   int num_idxs = 0;

   client = mongoc_client_new (gTestUri);
   ASSERT (client);

   collection = get_test_collection (client, "test_get_index_info");
   ASSERT (collection);

   /*
    * Try it on a collection that doesn't exist.
    */
   cursor = mongoc_collection_find_indexes (collection, &error);

   ASSERT (cursor);
   ASSERT (!error.domain);
   ASSERT (!error.code);

   ASSERT (!mongoc_cursor_next( cursor, &indexinfo ));

   mongoc_cursor_destroy (cursor);

   /* insert a dummy document so that the collection actually exists */
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, &dummy, NULL,
                                 &error);
   ASSERT (r);

   /* Try it on a collection with no secondary indexes.
    * We should just get back the index on _id.
    */
   cursor = mongoc_collection_find_indexes (collection, &error);
   ASSERT (cursor);
   ASSERT (!error.domain);
   ASSERT (!error.code);

   while (mongoc_cursor_next (cursor, &indexinfo)) {
      if (bson_iter_init (&idx_spec_iter, indexinfo) &&
          bson_iter_find (&idx_spec_iter, "name") &&
          BSON_ITER_HOLDS_UTF8 (&idx_spec_iter) &&
          (cur_idx_name = bson_iter_utf8 (&idx_spec_iter, NULL))) {
         assert (0 == strcmp (cur_idx_name, id_idx_name));
         ++num_idxs;
      } else {
         assert (false);
      }
   }

   assert (1 == num_idxs);

   mongoc_cursor_destroy (cursor);

   num_idxs = 0;
   indexinfo = NULL;

   bson_init (&indexkey1);
   BSON_APPEND_INT32 (&indexkey1, "rasberry", 1);
   idx1_name = mongoc_collection_keys_to_index_string (&indexkey1);
   mongoc_index_opt_init (&opt1);
   opt1.background = true;
   r = mongoc_collection_create_index (collection, &indexkey1, &opt1, &error);
   ASSERT (r);

   bson_init (&indexkey2);
   BSON_APPEND_INT32 (&indexkey2, "snozzberry", 1);
   idx2_name = mongoc_collection_keys_to_index_string (&indexkey2);
   mongoc_index_opt_init (&opt2);
   opt2.unique = true;
   r = mongoc_collection_create_index (collection, &indexkey2, &opt2, &error);
   ASSERT (r);

   /*
    * Now we try again after creating two indexes.
    */
   cursor = mongoc_collection_find_indexes (collection, &error);
   ASSERT (cursor);
   ASSERT (!error.domain);
   ASSERT (!error.code);

   while (mongoc_cursor_next (cursor, &indexinfo)) {
      if (bson_iter_init (&idx_spec_iter, indexinfo) &&
          bson_iter_find (&idx_spec_iter, "name") &&
          BSON_ITER_HOLDS_UTF8 (&idx_spec_iter) &&
          (cur_idx_name = bson_iter_utf8 (&idx_spec_iter, NULL))) {
         if (0 == strcmp (cur_idx_name, idx1_name)) {
            /* need to use the copy of the iter since idx_spec_iter may have gone
             * past the key we want */
            ASSERT (bson_iter_init_find (&idx_spec_iter_copy, indexinfo, "background"));
            ASSERT (BSON_ITER_HOLDS_BOOL (&idx_spec_iter_copy));
            ASSERT (bson_iter_bool (&idx_spec_iter_copy));
         } else if (0 == strcmp (cur_idx_name, idx2_name)) {
            ASSERT (bson_iter_init_find (&idx_spec_iter_copy, indexinfo, "unique"));
            ASSERT (BSON_ITER_HOLDS_BOOL (&idx_spec_iter_copy));
            ASSERT (bson_iter_bool (&idx_spec_iter_copy));
         } else {
            ASSERT ((0 == strcmp (cur_idx_name, id_idx_name)));
         }

         ++num_idxs;
      } else {
         assert (false);
      }
   }

   assert (3 == num_idxs);

   mongoc_cursor_destroy (cursor);

   bson_free (idx1_name);
   bson_free (idx2_name);

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}

static void
cleanup_globals (void)
{
   bson_free (gTestUri);
}


void
test_collection_install (TestSuite *suite)
{
   gTestUri = bson_strdup_printf("mongodb://%s/", MONGOC_TEST_HOST);

   TestSuite_Add (suite, "/Collection/insert_bulk", test_insert_bulk);
   TestSuite_Add (suite, "/Collection/insert", test_insert);
   TestSuite_Add (suite, "/Collection/save", test_save);
   TestSuite_Add (suite, "/Collection/index", test_index);
   TestSuite_Add (suite, "/Collection/index_compound", test_index_compound);
   TestSuite_Add (suite, "/Collection/index_geo", test_index_geo);
   TestSuite_Add (suite, "/Collection/index_storage", test_index_storage);
   TestSuite_Add (suite, "/Collection/regex", test_regex);
   TestSuite_Add (suite, "/Collection/update", test_update);
   TestSuite_Add (suite, "/Collection/remove", test_remove);
   TestSuite_Add (suite, "/Collection/count", test_count);
   TestSuite_Add (suite, "/Collection/count_with_opts", test_count_with_opts);
   TestSuite_Add (suite, "/Collection/drop", test_drop);
   TestSuite_Add (suite, "/Collection/aggregate", test_aggregate);
   TestSuite_Add (suite, "/Collection/validate", test_validate);
   TestSuite_Add (suite, "/Collection/rename", test_rename);
   TestSuite_Add (suite, "/Collection/stats", test_stats);
   TestSuite_Add (suite, "/Collection/find_and_modify", test_find_and_modify);
   TestSuite_Add (suite, "/Collection/large_return", test_large_return);
   TestSuite_Add (suite, "/Collection/many_return", test_many_return);
   TestSuite_Add (suite, "/Collection/command_fully_qualified", test_command_fq);
   TestSuite_Add (suite, "/Collection/get_index_info", test_get_index_info);

   atexit (cleanup_globals);
}
