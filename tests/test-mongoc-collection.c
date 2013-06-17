#include <mongoc.h>
#include <mongoc-log.h>

#include "mongoc-tests.h"


#define TEST_HOST "mongodb://127.0.0.1:27017/"


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

   return 0;
}
