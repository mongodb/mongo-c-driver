#include <mongoc.h>

#include "TestSuite.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "mock_server/mock-server.h"
#include "test-conveniences.h"


static void
_test_query (const mongoc_uri_t *uri,
             mock_server_t *server,
             const char *query_in,
             mongoc_read_prefs_t *read_prefs,
             mongoc_query_flags_t expected_query_flags,
             const char *expected_query)
{
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_t b = BSON_INITIALIZER;
   future_t *future;
   request_t *request;

   client = mongoc_client_new_from_uri (uri);
   collection = mongoc_client_get_collection (client, "test", "test");
   mongoc_collection_set_read_prefs (collection, read_prefs);

   cursor = mongoc_collection_find (collection,
                                    MONGOC_QUERY_NONE,
                                    0,
                                    1,
                                    0,
                                    tmp_bson (query_in),
                                    NULL,
                                    read_prefs);

   future = future_cursor_next (cursor, &doc);

   request = mock_server_receives_query (
      server,
      "test.test",
      expected_query_flags,
      0,
      0,
      expected_query,
      NULL);

   mock_server_replies (request,
                        0,                    /* flags */
                        0,                    /* cursorId */
                        0,                    /* startingFrom */
                        1,                    /* numberReturned */
                        "{'a': 1}");

   /* mongoc_cursor_next returned true */
   assert (future_get_bool (future));

   request_destroy (request);
   future_destroy (future);
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (&b);
}


typedef enum {
    READ_PREF_TEST_STANDALONE,
    READ_PREF_TEST_MONGOS,
    READ_PREF_TEST_SECONDARY,
} read_pref_test_type_t;


static void
_test_read_prefs (read_pref_test_type_t test_type,
                  mongoc_read_prefs_t *read_prefs,
                  mongoc_query_flags_t expected_query_flags,
                  const char *query_in,
                  const char *expected_query)
{
   mock_server_t *server;

   server = mock_server_new ();
   switch (test_type) {
   case READ_PREF_TEST_STANDALONE:
      mock_server_auto_ismaster (server, "{'ok': 1, 'ismaster': true}");
      break;
   case READ_PREF_TEST_MONGOS:
      mock_server_auto_ismaster (server, "{'ok': 1,"
                                         " 'ismaster': true,"
                                         " 'msg': 'isdbgrid'}");
      break;
   case READ_PREF_TEST_SECONDARY:
      mock_server_auto_ismaster (server, "{'ok': 1,"
                                         " 'ismaster': false,"
                                         " 'secondary': 'true'",
                                         " 'setName': 'rs'",
                                         " 'hosts': ['%s']}",
                                 mock_server_get_host_and_port (server));
      break;
   default:
      fprintf (stderr, "Invalid test_type: : %d\n", test_type);
      abort ();
   }

   mock_server_run (server);

   _test_query (mock_server_get_uri (server),
                server,
                query_in,
                read_prefs,
                expected_query_flags,
                expected_query);

   mock_server_destroy (server);
}


static void
test_read_prefs_standalone_primary (void)
{
   mongoc_read_prefs_t *read_prefs;

   /* Server Selection Spec: for topology type single and server types other
    * than mongos, "clients MUST always set the slaveOK wire protocol flag on
    * reads to ensure that any server type can handle the request."
    * */
   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   _test_read_prefs (READ_PREF_TEST_STANDALONE, read_prefs,
                     MONGOC_QUERY_SLAVE_OK, "{}", "{}");

   _test_read_prefs (READ_PREF_TEST_STANDALONE, read_prefs,
                     MONGOC_QUERY_SLAVE_OK, "{'a': 1}", "{'a': 1}");

   mongoc_read_prefs_destroy (read_prefs);
}


static void
test_read_prefs_standalone_secondary (void)
{
   mongoc_read_prefs_t *read_prefs;

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);

   _test_read_prefs (READ_PREF_TEST_STANDALONE, read_prefs,
                     MONGOC_QUERY_SLAVE_OK, "{}", "{}");

   _test_read_prefs (READ_PREF_TEST_STANDALONE, read_prefs,
                     MONGOC_QUERY_SLAVE_OK, "{'a': 1}", "{'a': 1}");

   mongoc_read_prefs_destroy (read_prefs);
}


static void
test_read_prefs_standalone_tags (void)
{
   bson_t b = BSON_INITIALIZER;
   mongoc_read_prefs_t *read_prefs;

   bson_append_utf8 (&b, "dc", 2, "ny", 2);

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY_PREFERRED);
   mongoc_read_prefs_add_tag (read_prefs, &b);
   mongoc_read_prefs_add_tag (read_prefs, NULL);

   _test_read_prefs (READ_PREF_TEST_STANDALONE, read_prefs,
                     MONGOC_QUERY_SLAVE_OK, "{}", "{}");

   _test_read_prefs (READ_PREF_TEST_STANDALONE, read_prefs,
                     MONGOC_QUERY_SLAVE_OK, "{'a': 1}", "{'a': 1}");

   mongoc_read_prefs_destroy (read_prefs);
}

static void
test_read_prefs_mongos_primary (void)
{
   mongoc_read_prefs_t *read_prefs;

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   _test_read_prefs (READ_PREF_TEST_MONGOS, read_prefs,
                     MONGOC_QUERY_NONE, "{}", "{}");

   _test_read_prefs (READ_PREF_TEST_MONGOS, read_prefs,
                     MONGOC_QUERY_NONE, "{'a': 1}", "{'a': 1}");

   mongoc_read_prefs_destroy (read_prefs);
}


static void
test_read_prefs_mongos_secondary (void)
{
   mongoc_read_prefs_t *read_prefs;

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);

   _test_read_prefs (
      READ_PREF_TEST_MONGOS, read_prefs,
      MONGOC_QUERY_SLAVE_OK,
      "{}",
      "{'$readPreference': {'mode': 'secondary'}}");

   _test_read_prefs (
      READ_PREF_TEST_MONGOS, read_prefs,
      MONGOC_QUERY_SLAVE_OK,
      "{'a': 1}",
      "{'$query': {'a': 1}, '$readPreference': {'mode': 'secondary'}}");

   _test_read_prefs (
      READ_PREF_TEST_MONGOS, read_prefs,
      MONGOC_QUERY_SLAVE_OK,
      "{'$query': {'a': 1}}",
      "{'$query': {'a': 1}, '$readPreference': {'mode': 'secondary'}}");

   mongoc_read_prefs_destroy (read_prefs);
}


static void
test_read_prefs_mongos_secondary_preferred (void)
{
   mongoc_read_prefs_t *read_prefs;

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY_PREFERRED);

   /* $readPreference not sent, only slaveOk */
   _test_read_prefs (READ_PREF_TEST_MONGOS, read_prefs,
                     MONGOC_QUERY_SLAVE_OK, "{}", "{}");

   _test_read_prefs (READ_PREF_TEST_MONGOS, read_prefs,
                     MONGOC_QUERY_SLAVE_OK, "{'a': 1}", "{'a': 1}");

   mongoc_read_prefs_destroy (read_prefs);
}


static void
test_read_prefs_mongos_tags (void)
{
   bson_t b = BSON_INITIALIZER;
   mongoc_read_prefs_t *read_prefs;

   bson_append_utf8 (&b, "dc", 2, "ny", 2);

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY_PREFERRED);
   mongoc_read_prefs_add_tag (read_prefs, &b);
   mongoc_read_prefs_add_tag (read_prefs, NULL);

   _test_read_prefs (
      READ_PREF_TEST_MONGOS, read_prefs,
      MONGOC_QUERY_SLAVE_OK,
      "{}",
      "{'$readPreference': {'mode': 'secondaryPreferred',"
      "                     'tags': [{'dc': 'ny'}, {}]}}");

   _test_read_prefs (
      READ_PREF_TEST_MONGOS, read_prefs,
      MONGOC_QUERY_SLAVE_OK,
      "{'a': 1}",
      "{'$query': {'a': 1},"
      " '$readPreference': {'mode': 'secondaryPreferred',"
      "                     'tags': [{'dc': 'ny'}, {}]}}`");

   mongoc_read_prefs_destroy (read_prefs);
}


static void
test_mongoc_read_prefs_score (void)
{
#if 0

   mongoc_read_prefs_t *read_prefs;
   bool valid;
   int score;

#define ASSERT_VALID(r) \
   valid = mongoc_read_prefs_is_valid(r); \
   ASSERT_CMPINT(valid, ==, 1)

   read_prefs = mongoc_read_prefs_new(MONGOC_READ_PRIMARY);

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_PRIMARY);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   ASSERT_CMPINT(score, ==, 0);

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_PRIMARY_PREFERRED);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   ASSERT_CMPINT(score, ==, 1);

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_SECONDARY_PREFERRED);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   ASSERT_CMPINT(score, ==, 1);

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_SECONDARY);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   ASSERT_CMPINT(score, ==, 1);

   mongoc_read_prefs_destroy(read_prefs);

#undef ASSERT_VALID
#endif
}


void
test_read_prefs_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/ReadPrefs/score", test_mongoc_read_prefs_score);
   TestSuite_Add (suite, "/ReadPrefs/standalone/primary",
                  test_read_prefs_standalone_primary);
   TestSuite_Add (suite, "/ReadPrefs/standalone/secondary",
                  test_read_prefs_standalone_secondary);
   TestSuite_Add (suite, "/ReadPrefs/standalone/tags",
                  test_read_prefs_standalone_tags);
   TestSuite_Add (suite, "/ReadPrefs/mongos/primary",
                  test_read_prefs_mongos_primary);
   TestSuite_Add (suite, "/ReadPrefs/mongos/secondary",
                  test_read_prefs_mongos_secondary);
   TestSuite_Add (suite, "/ReadPrefs/mongos/secondaryPreferred",
                  test_read_prefs_mongos_secondary_preferred);
   TestSuite_Add (suite, "/ReadPrefs/mongos/tags",
                  test_read_prefs_mongos_tags);
}
