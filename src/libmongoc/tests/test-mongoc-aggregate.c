#include <mongoc/mongoc.h>
#include "mongoc/mongoc-client-private.h"

#include "TestSuite.h"
#include "mock_server/mock-server.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "test-conveniences.h"

static void
_test_tailable_query_flag (bool flag)
{
   mock_server_t *server;
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   bson_error_t error;
   future_t *future;
   request_t *request;
   const bson_t *doc;

   server = mock_server_with_autoismaster (WIRE_VERSION_MAX);
   mock_server_run (server);
   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
   collection = mongoc_client_get_collection (client, "db", "collection");
   cursor = mongoc_collection_aggregate (
      collection,
      flag ? MONGOC_QUERY_TAILABLE_CURSOR : MONGOC_QUERY_NONE,
      tmp_bson ("{'pipeline': []}"),
      flag ? NULL : tmp_bson ("{'tailable': true}"),
      NULL);

   ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);

   /* "aggregate" command */
   future = future_cursor_next (cursor, &doc);
   request =
      mock_server_receives_msg (server,
                                MONGOC_QUERY_NONE,
                                tmp_bson ("{'aggregate': 'collection',"
                                          " 'pipeline': [ ],"
                                          " 'tailable': {'$exists': false}}"));
   ASSERT (request);
   mock_server_replies_simple (request,
                               "{'ok': 1,"
                               " 'cursor': {"
                               "    'id': {'$numberLong': '123'},"
                               "    'ns': 'db.collection',"
                               "    'nextBatch': [{}]}}");
   ASSERT (future_get_bool (future));
   future_destroy (future);

   /* "getMore" command */
   future = future_cursor_next (cursor, &doc);
   request =
      mock_server_receives_msg (server,
                                MONGOC_QUERY_NONE,
                                tmp_bson ("{'getMore': {'$numberLong': '123'},"
                                          " 'collection': 'collection',"
                                          " 'tailable': {'$exists': false}}"));
   ASSERT (request);
   mock_server_replies_simple (request,
                               "{'ok': 1,"
                               " 'cursor': {"
                               "    'id': {'$numberLong': '0'},"
                               "    'ns': 'db.collection',"
                               "    'nextBatch': [{}]}}");

   ASSERT (future_get_bool (future));

   request_destroy (request);
   future_destroy (future);
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}

static void
test_tailable_query_flag (void)
{
   _test_tailable_query_flag (true);
   _test_tailable_query_flag (false);
}

void
test_aggregate_install (TestSuite *suite)
{
   TestSuite_AddMockServerTest (
      suite, "/Aggregate/tailable_query_flag", test_tailable_query_flag);
}