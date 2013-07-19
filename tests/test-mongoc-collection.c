#include <mongoc.h>
#include <mongoc-log.h>

#include "mongoc-tests.h"


#define TEST_HOST (getenv("MONGOC_TEST_URI") ?: "mongodb://127.0.0.1:27017/")


static void
test_insert (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_context_t *context;
   bson_error_t error;
   bson_bool_t r;
   bson_oid_t oid;
   unsigned i;
   bson_t b;

   client = mongoc_client_new(TEST_HOST);
   assert(client);

   collection = mongoc_client_get_collection(client, "test", "test");
   assert(collection);

   context = bson_context_new(BSON_CONTEXT_NONE);
   assert(context);

   for (i = 0; i < 10; i++) {
      bson_init(&b);
      bson_oid_init(&oid, context);
      bson_append_oid(&b, "_id", 3, &oid);
      bson_append_utf8(&b, "hello", 5, "world", 5);
      r = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &b, NULL,
                                   &error);
      if (!r) {
         MONGOC_WARNING("%s\n", error.message);
         bson_error_destroy(&error);
      }
      assert(r);
      bson_destroy(&b);
   }

   mongoc_collection_destroy(collection);
   bson_context_destroy(context);
   mongoc_client_destroy(client);
}


static void
test_update (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_context_t *context;
   bson_error_t error;
   bson_bool_t r;
   bson_oid_t oid;
   unsigned i;
   bson_t b;
   bson_t q;
   bson_t u;
   bson_t set;

   client = mongoc_client_new(TEST_HOST);
   assert(client);

   collection = mongoc_client_get_collection(client, "test", "test");
   assert(collection);

   context = bson_context_new(BSON_CONTEXT_NONE);
   assert(context);

   for (i = 0; i < 10; i++) {
      bson_init(&b);
      bson_oid_init(&oid, context);
      bson_append_oid(&b, "_id", 3, &oid);
      bson_append_utf8(&b, "utf8", 4, "utf8 string", sizeof "utf8 string");
      bson_append_int32(&b, "int32", 5, 1234);
      bson_append_int64(&b, "int64", 5, 12345678);
      bson_append_bool(&b, "bool", 4, 1);

      r = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &b, NULL, &error);
      if (!r) {
         MONGOC_WARNING("%s\n", error.message);
         bson_error_destroy(&error);
      }
      assert(r);

      bson_init(&q);
      bson_append_oid(&q, "_id", 3, &oid);

      bson_init(&u);
      bson_append_document_begin(&u, "$set", 4, &set);
      bson_append_utf8(&set, "utf8", 4, "updated", 7);
      bson_append_document_end(&u, &set);

      r = mongoc_collection_update(collection, MONGOC_UPDATE_NONE, &q, &u, NULL, &error);
      if (!r) {
         MONGOC_WARNING("%s\n", error.message);
         bson_error_destroy(&error);
      }
      assert(r);

      bson_destroy(&b);
      bson_destroy(&q);
   }

   mongoc_collection_destroy(collection);
   bson_context_destroy(context);
   mongoc_client_destroy(client);
}


static void
test_delete (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_context_t *context;
   bson_error_t error;
   bson_bool_t r;
   bson_oid_t oid;
   bson_t b;
   int i;

   client = mongoc_client_new(TEST_HOST);
   assert(client);

   collection = mongoc_client_get_collection(client, "test", "test");
   assert(collection);

   context = bson_context_new(BSON_CONTEXT_NONE);
   assert(context);

   for (i = 0; i < 100; i++) {
      bson_init(&b);
      bson_oid_init(&oid, context);
      bson_append_oid(&b, "_id", 3, &oid);
      bson_append_utf8(&b, "hello", 5, "world", 5);
      r = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &b, NULL,
                                   &error);
      if (!r) {
         MONGOC_WARNING("%s\n", error.message);
         bson_error_destroy(&error);
      }
      assert(r);
      bson_destroy(&b);

      bson_init(&b);
      bson_append_oid(&b, "_id", 3, &oid);
      r = mongoc_collection_delete(collection, MONGOC_DELETE_NONE, &b, NULL,
                                   &error);
      if (!r) {
         MONGOC_WARNING("%s\n", error.message);
         bson_error_destroy(&error);
      }
      assert(r);
      bson_destroy(&b);
   }

   mongoc_collection_destroy(collection);
   bson_context_destroy(context);
   mongoc_client_destroy(client);
}

static void
test_count (void)
{
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error;
   bson_int64_t count;
   bson_t b;

   client = mongoc_client_new(TEST_HOST);
   assert(client);

   collection = mongoc_client_get_collection(client, "test", "test");
   assert(collection);

   bson_init(&b);
   count = mongoc_collection_count(collection, MONGOC_QUERY_NONE, &b,
                                   0, 0, NULL, &error);
   bson_destroy(&b);

   if (count == -1) {
      MONGOC_WARNING("%s\n", error.message);
      bson_error_destroy(&error);
   }
   assert(count != -1);

   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
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

   run_test("/mongoc/collection/insert", test_insert);
   run_test("/mongoc/collection/update", test_update);
   run_test("/mongoc/collection/delete", test_delete);
   run_test("/mongoc/collection/count", test_count);

   return 0;
}
