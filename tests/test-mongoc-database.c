#include <mongoc.h>
#include <mongoc-client-private.h>
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
test_command (void)
{
   mongoc_database_t *database;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   bson_error_t error;
   const bson_t *doc;
   bson_bool_t r;
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply;

   client = mongoc_client_new (gTestUri);
   assert (client);

   database = mongoc_client_get_database (client, "admin");

   /*
    * Test a known working command, "ping".
    */
   bson_append_int32 (&cmd, "ping", 4, 1);

   cursor = mongoc_database_command (database, MONGOC_QUERY_NONE, 0, 1, 0, &cmd, NULL, NULL);
   assert (cursor);

   r = mongoc_cursor_next (cursor, &doc);
   assert (r);
   assert (doc);

   r = mongoc_cursor_next (cursor, &doc);
   assert (!r);
   assert (!doc);

   mongoc_cursor_destroy (cursor);


   /*
    * Test a non-existing command to ensure we get the failure.
    */
   bson_reinit (&cmd);
   bson_append_int32 (&cmd, "a_non_existing_command", -1, 1);

   r = mongoc_database_command_simple (database, &cmd, NULL, &reply, &error);
   assert (!r);
   assert (error.domain == MONGOC_ERROR_QUERY);
   assert (error.code == MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND);
   assert (!strcmp ("no such cmd: a_non_existing_command", error.message));

   mongoc_database_destroy (database);
   mongoc_client_destroy (client);
   bson_destroy (&cmd);
}


static void
test_drop (void)
{
   mongoc_database_t *database;
   mongoc_client_t *client;
   bson_error_t error = { 0 };
   bson_bool_t r;

   client = mongoc_client_new (gTestUri);
   assert (client);

   database = mongoc_client_get_database (client, "some_random_database");

   r = mongoc_database_drop (database, &error);
   assert (r);
   assert (!error.domain);
   assert (!error.code);

   mongoc_database_destroy (database);
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
   run_test("/mongoc/database/command", test_command);
   run_test("/mongoc/database/drop", test_drop);

   bson_free(gTestUri);

   return 0;
}
