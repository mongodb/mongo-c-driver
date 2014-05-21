#include <mongoc.h>

#include "TestSuite.h"
#include "test-libmongoc.h"
#include "mongoc-tests.h"

static char *gTestUri;


static void
test_has_collection (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   bson_error_t error;
   char *name;
   bool r;
   bson_oid_t oid;
   bson_t b;

   client = mongoc_client_new (gTestUri);
   assert (client);

   name = gen_collection_name ("has_collection");
   collection = mongoc_client_get_collection (client, "test", name);
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

   r = mongoc_database_has_collection (database, name, &error);
   assert (!error.domain);
   assert (r);

   bson_free (name);
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
   bool r;
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
   char *dbname;
   bool r;

   client = mongoc_client_new (gTestUri);
   assert (client);

   dbname = gen_collection_name ("db_drop_test");
   database = mongoc_client_get_database (client, dbname);
   bson_free (dbname);

   r = mongoc_database_drop (database, &error);
   assert (r);
   assert (!error.domain);
   assert (!error.code);

   mongoc_database_destroy (database);
   mongoc_client_destroy (client);
}


static void
test_create_collection (void)
{
   mongoc_database_t *database;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error = { 0 };
   bson_t options;
   char *dbname;
   char *name;
   bool r;

   client = mongoc_client_new (gTestUri);
   assert (client);

   dbname = gen_collection_name ("dbtest");
   database = mongoc_client_get_database (client, dbname);
   assert (database);
   bson_free (dbname);

   bson_init (&options);
   BSON_APPEND_INT32 (&options, "size", 1234);
   BSON_APPEND_INT32 (&options, "max", 4567);
   BSON_APPEND_BOOL (&options, "capped", true);
   BSON_APPEND_BOOL (&options, "autoIndexId", true);

   name = gen_collection_name ("create_collection");
   collection = mongoc_database_create_collection (database, name, &options, &error);
   assert (collection);
   bson_free (name);

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   r = mongoc_database_drop (database, &error);
   assert (r);

   mongoc_collection_destroy (collection);
   mongoc_database_destroy (database);
   mongoc_client_destroy (client);
}


static void
cleanup_globals (void)
{
   bson_free (gTestUri);
}


void
test_database_install (TestSuite *suite)
{
   gTestUri = bson_strdup_printf ("mongodb://%s/", MONGOC_TEST_HOST);

   TestSuite_Add (suite, "/Database/has_collection", test_has_collection);
   TestSuite_Add (suite, "/Database/command", test_command);
   TestSuite_Add (suite, "/Database/drop", test_drop);
   TestSuite_Add (suite, "/Database/create_collection", test_create_collection);

   atexit (cleanup_globals);
}
