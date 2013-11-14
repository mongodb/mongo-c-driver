#include <mongoc.h>
#include <mongoc-log.h>

#include "mongoc-tests.h"


#define HOST (getenv("MONGOC_TEST_HOST") ? getenv("MONGOC_TEST_HOST") : "localhost")


static char *gTestUri;


static void
test_has_collection (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   bson_error_t error;
   bson_bool_t r;
   bson_oid_t oid;
   bson_t b;

   client = mongoc_client_new (gTestUri);
   assert (client);

   collection = mongoc_client_get_collection (client, "test", "test");
   assert (collection);

   database = mongoc_client_get_database (client, "test");
   assert (database);

   bson_init (&b);
   bson_oid_init (&oid, NULL);
   bson_append_oid (&b, "_id", 3, &oid);
   bson_append_utf8 (&b, "hello", 5, "world", 5);
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, &b, NULL,
                                 &error);
   if (!r) {
      MONGOC_WARNING ("%s\n", error.message);
   }
   assert (r);
   bson_destroy (&b);

   r = mongoc_database_has_collection (database, "test", &error);
   assert (!error.domain);
   assert (r);

   mongoc_database_destroy (database);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
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

   gTestUri = bson_strdup_printf("mongodb://%s/", HOST);

   run_test("/mongoc/database/has_collection", test_has_collection);

   bson_free(gTestUri);

   return 0;
}
