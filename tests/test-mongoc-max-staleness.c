#include <mongoc.h>
#include <mongoc-util-private.h>

#include "mongoc-client-private.h"

#include "TestSuite.h"
#include "json-test.h"
#include "test-libmongoc.h"
#include "mock_server/mock-server.h"
#include "mock_server/future-functions.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "client-test-max-staleness"


/* the next few tests are from max-staleness-tests.rst */
static void
test_mongoc_client_max_staleness (void)
{
   mongoc_client_t *client;
   int32_t max_staleness_ms;

   /* no maxStalenessMS with primary mode */
   ASSERT (!mongoc_client_new ("mongodb://a/?maxStalenessMS=120000"));
   ASSERT (!mongoc_client_new (
              "mongodb://a/?readPreference=primary&maxStalenessMS=120000"));

   client = mongoc_client_new (
      "mongodb://host/?readPreference=secondary&maxStalenessMS=120000");
   max_staleness_ms = mongoc_uri_get_option_as_int32 (
      mongoc_client_get_uri (client), "maxstalenessms", 0);
   ASSERT_CMPINT32 (120000, ==, max_staleness_ms);
   mongoc_client_destroy (client);

   client = mongoc_client_new (
      "mongodb://a/?readPreference=secondary&maxStalenessMS=1");
   max_staleness_ms = mongoc_uri_get_option_as_int32 (
      mongoc_client_get_uri (client), "maxstalenessms", 0);
   ASSERT_CMPINT32 (1, ==, max_staleness_ms);
   mongoc_client_destroy (client);
}


static void
test_mongos_max_staleness_read_pref (void)
{
   mock_server_t *server;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   mongoc_read_prefs_t *prefs;
   future_t *future;
   request_t *request;
   bson_error_t error;

   server = mock_mongos_new (5 /* maxWireVersion */);
   mock_server_run (server);
   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
   collection = mongoc_client_get_collection (client, "db", "collection");

   /* count command with mode "secondary", no maxStalenessMS */
   prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   mongoc_collection_set_read_prefs (collection, prefs);
   future = future_collection_count (collection, MONGOC_QUERY_NONE,
                                     NULL, 0, 0, NULL, &error);
   request = mock_server_receives_command (
      server, "db", MONGOC_QUERY_SLAVE_OK,
      "{'$readPreference': {'mode': 'secondary', "
      "                     'maxStalenessMS': {'$exists': false}}}");

   mock_server_replies_simple (request, "{'ok': 1, 'n': 1}");
   ASSERT_OR_PRINT (1 == future_get_int64_t (future), error);

   request_destroy (request);
   future_destroy (future);

   /* count command with mode "secondary", maxStalenessMS = 120 seconds */
   mongoc_read_prefs_set_max_staleness_ms (prefs, 120000);
   mongoc_collection_set_read_prefs (collection, prefs);

   mongoc_collection_set_read_prefs (collection, prefs);
   future = future_collection_count (collection, MONGOC_QUERY_NONE,
                                     NULL, 0, 0, NULL, &error);
   request = mock_server_receives_command (
      server, "db", MONGOC_QUERY_SLAVE_OK,
      "{'$readPreference': {'mode': 'secondary', 'maxStalenessMS': 120000}}");

   mock_server_replies_simple (request, "{'ok': 1, 'n': 1}");
   ASSERT_OR_PRINT (1 == future_get_int64_t (future), error);

   request_destroy (request);
   future_destroy (future);

   mongoc_read_prefs_destroy (prefs);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}


static void
_test_last_write_date (bool pooled)
{
   mongoc_uri_t *uri;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   bson_error_t error;
   bool r;
   mongoc_server_description_t *s0, *s1;
   int64_t delta;

   uri = test_framework_get_uri ();
   mongoc_uri_set_option_as_int32 (uri, "heartbeatFrequencyMS", 500);

   if (pooled) {
      pool = mongoc_client_pool_new (uri);
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (uri);
   }

   collection = get_test_collection (client, "test_last_write_date");
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE,
                                 tmp_bson ("{}"), NULL, &error);
   ASSERT_OR_PRINT (r, error);

   _mongoc_usleep (1000 * 1000);
   s0 = mongoc_topology_select (client->topology, MONGOC_SS_READ, NULL, &error);
   ASSERT_OR_PRINT (s0, error);

   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE,
                                 tmp_bson ("{}"), NULL, &error);
   ASSERT_OR_PRINT (r, error);

   _mongoc_usleep (1000 * 1000);
   s1 = mongoc_topology_select (client->topology, MONGOC_SS_READ, NULL, &error);
   ASSERT_OR_PRINT (s1, error);

   /* lastWriteDate increased by roughly one second - be lenient, just check
    * it increased by less than 10 seconds */
   delta = s1->last_write_date_ms - s0->last_write_date_ms;
   ASSERT_CMPINT64 (delta, >, (int64_t) 0);
   ASSERT_CMPINT64 (delta, <, (int64_t) 10 * 1000);

   mongoc_server_description_cleanup (s0);
   mongoc_server_description_cleanup (s1);
   mongoc_collection_destroy (collection);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }
}


static void
test_last_write_date (void *ctx)
{
   _test_last_write_date (false);
}


static void
test_last_write_date_pooled (void *ctx)
{
   _test_last_write_date (true);
}


/* run only if wire version is older than 5 */
static void
_test_last_write_date_absent (bool pooled)
{
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_server_description_t *sd;

   if (pooled) {
      pool = test_framework_client_pool_new ();
      client = mongoc_client_pool_pop (pool);
   } else {
      client = test_framework_client_new ();
   }

   sd = mongoc_topology_select (client->topology, MONGOC_SS_READ, NULL, &error);
   ASSERT_OR_PRINT (sd, error);

   /* lastWriteDate absent */
   ASSERT_CMPINT64 (sd->last_write_date_ms, ==, (int64_t) -1);

   mongoc_server_description_cleanup (sd);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }
}



static void
test_last_write_date_absent (void *ctx)
{
   _test_last_write_date_absent (false);
}


static void
test_last_write_date_absent_pooled (void *ctx)
{
   _test_last_write_date_absent (true);
}


static void
test_all_spec_tests (TestSuite *suite)
{
   char resolved[PATH_MAX];

   assert (realpath ("tests/json/max_staleness", resolved));
   install_json_test_suite (suite, resolved, &test_server_selection_logic_cb);
}

void
test_client_max_staleness_install (TestSuite *suite)
{
   test_all_spec_tests (suite);
   TestSuite_Add (suite, "/Client/max_staleness",
                  test_mongoc_client_max_staleness);
   TestSuite_Add (suite, "/Client/max_staleness/mongos",
                  test_mongos_max_staleness_read_pref);
   TestSuite_AddFull (suite, "/Client/last_write_date",
                      test_last_write_date, NULL, NULL,
                      test_framework_skip_if_max_version_version_less_than_5);
   TestSuite_AddFull (suite, "/Client/last_write_date/pooled",
                      test_last_write_date_pooled, NULL, NULL,
                      test_framework_skip_if_max_version_version_less_than_5);
   TestSuite_AddFull (suite, "/Client/last_write_date_absent",
                      test_last_write_date_absent, NULL, NULL,
                      test_framework_skip_if_max_version_version_more_than_4);
   TestSuite_AddFull (suite, "/Client/last_write_date_absent/pooled",
                      test_last_write_date_absent_pooled, NULL, NULL,
                      test_framework_skip_if_max_version_version_more_than_4);
}
