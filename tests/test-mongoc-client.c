#include <mongoc.h>

#include "mongoc-tests.h"


#define HOSTENV "MONGOC_TEST_HOST"
#define HOST (getenv(HOSTENV) ? getenv(HOSTENV) : "localhost")


static char *gTestUri;
static char *gTestUriWithPassword;
static char *gTestUriWithBadPassword;


static void
test_mongoc_client_authenticate (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_error_t error;
   bson_bool_t r;
   bson_t q;

   /*
    * Add a user to the test database.
    */
   client = mongoc_client_new(gTestUri);
   database = mongoc_client_get_database(client, "test");
   r = mongoc_database_add_user(database, "testuser", "testpass", &error);
   assert_cmpint(r, ==, 1);
   mongoc_database_destroy(database);
   mongoc_client_destroy(client);

   /*
    * Try authenticating with that user.
    */
   bson_init(&q);
   client = mongoc_client_new(gTestUriWithPassword);
   collection = mongoc_client_get_collection(client, "test", "test");
   cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1,
                                   &q, NULL, NULL);
   r = mongoc_cursor_next(cursor, &doc);
   if (!r) {
      r = mongoc_cursor_error(cursor, &error);
      if (r) MONGOC_ERROR("%s", error.message);
      assert(!r);
   }
   mongoc_cursor_destroy(cursor);
   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
}


static void
test_mongoc_client_authenticate_failure (void)
{
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   const bson_t *doc;
   bson_error_t error;
   bson_bool_t r;
   bson_t q;

   /*
    * Try authenticating with that user.
    */
   bson_init(&q);
   client = mongoc_client_new(gTestUriWithBadPassword);
   collection = mongoc_client_get_collection(client, "test", "test");
   cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1,
                                   &q, NULL, NULL);
   r = mongoc_cursor_next(cursor, &doc);
   assert(!r);
   r = mongoc_cursor_error(cursor, &error);
   assert(r);
   assert(error.domain == MONGOC_ERROR_CLIENT);
   assert(error.code == MONGOC_ERROR_CLIENT_AUTHENTICATE);
   mongoc_cursor_destroy(cursor);
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

   gTestUri = bson_strdup_printf("mongodb://%s:27017/", HOST);
   gTestUriWithPassword = bson_strdup_printf("mongodb://testuser:testpass@%s:27017/test", HOST);
   gTestUriWithBadPassword = bson_strdup_printf("mongodb://baduser:badpass@%s:27017/test", HOST);

   run_test("/mongoc/client/authenticate", test_mongoc_client_authenticate);
   run_test("/mongoc/client/authenticate_failure", test_mongoc_client_authenticate_failure);

   bson_free(gTestUri);
   bson_free(gTestUriWithPassword);
   bson_free(gTestUriWithBadPassword);

   return 0;
}
