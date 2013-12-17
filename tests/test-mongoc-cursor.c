#include <mongoc.h>
#include <mongoc-cursor-private.h>

#include "mongoc-tests.h"

#define HOST (getenv("MONGOC_TEST_HOST") ? getenv("MONGOC_TEST_HOST") : "localhost")

static void
test_get_host (void)
{
   const mongoc_host_list_t *hosts;
   mongoc_host_list_t host;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   mongoc_uri_t *uri;
   const bson_t *doc;
   bson_error_t error;
   bson_bool_t r;
   bson_t q = BSON_INITIALIZER;
   char *uristr;

   uristr = bson_strdup_printf("mongodb://%s/", HOST);
   uri = mongoc_uri_new(uristr);
   bson_free(uristr);

   hosts = mongoc_uri_get_hosts(uri);

   client = mongoc_client_new_from_uri(uri);
   cursor = _mongoc_cursor_new(client, "test.test", MONGOC_QUERY_NONE, 0, 1, 1,
                               FALSE, &q, NULL, NULL);
   r = mongoc_cursor_next(cursor, &doc);
   if (!r && mongoc_cursor_error(cursor, &error)) {
      MONGOC_ERROR("%s", error.message);
      abort();
   }

   mongoc_cursor_get_host(cursor, &host);
   assert_cmpstr(host.host, hosts->host);
   assert_cmpstr(host.host_and_port, hosts->host_and_port);
   assert_cmpint(host.port, ==, hosts->port);
   assert_cmpint(host.family, ==, hosts->family);

   mongoc_uri_destroy(uri);
}

static void
test_clone (void)
{
   mongoc_cursor_t *clone;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   const bson_t *doc;
   bson_error_t error;
   mongoc_uri_t *uri;
   bson_bool_t r;
   bson_t q = BSON_INITIALIZER;
   char *uristr;

   uristr = bson_strdup_printf("mongodb://%s/", HOST);
   uri = mongoc_uri_new(uristr);
   bson_free(uristr);

   client = mongoc_client_new_from_uri(uri);
   BSON_ASSERT(client);

   {
      /*
       * Ensure test.test has a document.
       */

      mongoc_collection_t *col;

      col = mongoc_client_get_collection (client, "test", "test");
      r = mongoc_collection_insert (col, MONGOC_INSERT_NONE, &q, NULL, &error);
      BSON_ASSERT (r);

      mongoc_collection_destroy (col);
   }

   cursor = _mongoc_cursor_new(client, "test.test", MONGOC_QUERY_NONE, 0, 1, 1,
                               FALSE, &q, NULL, NULL);
   BSON_ASSERT(cursor);

   r = mongoc_cursor_next(cursor, &doc);
   if (!r || mongoc_cursor_error(cursor, &error)) {
      MONGOC_ERROR("%s", error.message);
      abort();
   }
   BSON_ASSERT (doc);

   clone = mongoc_cursor_clone(cursor);
   BSON_ASSERT(cursor);

   r = mongoc_cursor_next(clone, &doc);
   if (!r || mongoc_cursor_error(clone, &error)) {
      MONGOC_ERROR("%s", error.message);
      abort();
   }
   BSON_ASSERT (doc);

   mongoc_cursor_destroy(cursor);
   mongoc_cursor_destroy(clone);
   mongoc_client_destroy(client);
   mongoc_uri_destroy(uri);
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

   run_test("/mongoc/cursor/get_host", test_get_host);
   run_test("/mongoc/cursor/clone", test_clone);

   return 0;
}
