#include <mongoc.h>
#include <mongoc-cursor-private.h>
#include <assert.h>

#include "TestSuite.h"
#include "test-libmongoc.h"
#include "mock_server/mock-rs.h"
#include "mock_server/future-functions.h"
#include "test-conveniences.h"

static void
test_get_host (void)
{
   const mongoc_host_list_t *hosts;
   mongoc_host_list_t host;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   char *uri_str = test_framework_get_uri_str ();
   mongoc_uri_t *uri;
   const bson_t *doc;
   bson_error_t error;
   bool r;
   bson_t q = BSON_INITIALIZER;

   uri = mongoc_uri_new (uri_str);

   hosts = mongoc_uri_get_hosts(uri);

   client = test_framework_client_new ();
   cursor = _mongoc_cursor_new(client, "test.test", MONGOC_QUERY_NONE, 0, 1, 1,
                               false, &q, NULL, NULL);
   r = mongoc_cursor_next(cursor, &doc);
   if (!r && mongoc_cursor_error(cursor, &error)) {
      MONGOC_ERROR("%s", error.message);
      abort();
   }

   assert (doc == mongoc_cursor_current (cursor));

   mongoc_cursor_get_host(cursor, &host);
   ASSERT_CMPSTR (host.host, hosts->host);
   ASSERT_CMPSTR (host.host_and_port, hosts->host_and_port);
   ASSERT_CMPINT (host.port, ==, hosts->port);
   ASSERT_CMPINT (host.family, ==, hosts->family);

   bson_free (uri_str);
   mongoc_uri_destroy(uri);
   mongoc_client_destroy (client);
   mongoc_cursor_destroy (cursor);
}

static void
test_clone (void)
{
   mongoc_cursor_t *clone;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   const bson_t *doc;
   bson_error_t error;
   bool r;
   bson_t q = BSON_INITIALIZER;

   client = test_framework_client_new ();

   {
      /*
       * Ensure test.test has a document.
       */

      mongoc_collection_t *col;

      col = mongoc_client_get_collection (client, "test", "test");
      r = mongoc_collection_insert (col, MONGOC_INSERT_NONE, &q, NULL, &error);
      ASSERT (r);

      mongoc_collection_destroy (col);
   }

   cursor = _mongoc_cursor_new(client, "test.test", MONGOC_QUERY_NONE, 0, 1, 1,
                               false, &q, NULL, NULL);
   ASSERT(cursor);

   r = mongoc_cursor_next(cursor, &doc);
   if (!r || mongoc_cursor_error(cursor, &error)) {
      MONGOC_ERROR("%s", error.message);
      abort();
   }
   ASSERT (doc);

   clone = mongoc_cursor_clone(cursor);
   ASSERT(cursor);

   r = mongoc_cursor_next(clone, &doc);
   if (!r || mongoc_cursor_error(clone, &error)) {
      MONGOC_ERROR("%s", error.message);
      abort();
   }
   ASSERT (doc);

   mongoc_cursor_destroy(cursor);
   mongoc_cursor_destroy(clone);
   mongoc_client_destroy(client);
}


static void
test_invalid_query (void)
{
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   bson_error_t error;
   const bson_t *doc = NULL;
   bson_t *q;
   bool r;

   client = test_framework_client_new ();
   assert (client);

   q = BCON_NEW ("foo", BCON_INT32 (1), "$orderby", "{", "}");

   cursor = _mongoc_cursor_new (client, "test.test", MONGOC_QUERY_NONE, 0, 1, 1,
                                false, q, NULL, NULL);
   r = mongoc_cursor_next (cursor, &doc);
   assert (!r);
   mongoc_cursor_error (cursor, &error);
   assert (strstr (error.message, "$query"));
   assert (error.domain == MONGOC_ERROR_CURSOR);
   assert (error.code == MONGOC_ERROR_CURSOR_INVALID_CURSOR);
   assert (doc == NULL);

   bson_destroy (q);
   mongoc_cursor_destroy (cursor);
   mongoc_client_destroy (client);
}


static void
_test_kill_cursors (bool pooled)
{
   mock_rs_t *rs;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   bson_t *q = BCON_NEW ("a", BCON_INT32 (1));
   mongoc_read_prefs_t *prefs;
   mongoc_cursor_t *cursor;
   const bson_t *doc = NULL;
   future_t *future;
   request_t *request;
   bson_error_t error;
   request_t *kill_cursors;

   /* wire version 0, a primary, five secondaries, no arbiters */
   rs = mock_rs_with_autoismaster (0, true, 5, 0);
   mock_rs_run (rs);

   if (pooled) {
      pool = mongoc_client_pool_new (mock_rs_get_uri (rs));
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (mock_rs_get_uri (rs));
   }

   collection = mongoc_client_get_collection (client, "test", "test");

   prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   cursor = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0,
                                    q, NULL, prefs);

   future = future_cursor_next (cursor, &doc);
   request = mock_rs_receives_query (rs, "test.test", MONGOC_QUERY_SLAVE_OK,
                                     0, 0, "{'a': 1}", NULL);

   mock_rs_replies (request, 0, 123, 0, 1, "{'b': 1}");
   if (!future_get_bool (future)) {
      mongoc_cursor_error (cursor, &error);
      fprintf (stderr, "%s\n", error.message);
      abort ();
   };

   ASSERT_MATCH (doc, "{'b': 1}");
   ASSERT_CMPINT (123, ==, (int) mongoc_cursor_get_id (cursor));

   future_destroy (future);
   future = future_cursor_destroy (cursor);

   kill_cursors = mock_rs_receives_kill_cursors (rs, 123);

   /* OP_KILLCURSORS was sent to the right secondary */
   ASSERT_CMPINT (request_get_server_port (kill_cursors), ==,
                  request_get_server_port (request));

   assert (future_wait (future));

   request_destroy (kill_cursors);
   request_destroy (request);
   future_destroy (future);
   mongoc_read_prefs_destroy (prefs);
   mongoc_collection_destroy (collection);
   bson_destroy (q);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   mock_rs_destroy (rs);
}


static void
test_kill_cursors_single (void)
{
   _test_kill_cursors (false);
}


static void
test_kill_cursors_pooled (void)
{
   _test_kill_cursors (true);
}


static void
_test_getmore_fail (bool has_primary,
                    bool pooled)
{
   mock_rs_t *rs;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   bson_t *q = BCON_NEW ("a", BCON_INT32 (1));
   mongoc_read_prefs_t *prefs;
   mongoc_cursor_t *cursor;
   const bson_t *doc = NULL;
   future_t *future;
   request_t *request;

   /* wire version 0, five secondaries, no arbiters */
   rs = mock_rs_with_autoismaster (0, has_primary, 5, 0);
   mock_rs_run (rs);

   if (pooled) {
      pool = mongoc_client_pool_new (mock_rs_get_uri (rs));
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (mock_rs_get_uri (rs));
   }

   collection = mongoc_client_get_collection (client, "test", "test");

   prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   cursor = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0,
                                    q, NULL, prefs);

   future = future_cursor_next (cursor, &doc);
   request = mock_rs_receives_query (rs, "test.test", MONGOC_QUERY_SLAVE_OK,
                                     0, 0, "{'a': 1}", NULL);

   mock_rs_replies (request, 0, 123, 0, 1, "{'b': 1}");
   assert (future_get_bool (future));
   ASSERT_MATCH (doc, "{'b': 1}");
   ASSERT_CMPINT (123, ==, (int) mongoc_cursor_get_id (cursor));

   future_destroy (future);
   future = future_cursor_next (cursor, &doc);
   request = mock_rs_receives_getmore (rs, "test.test", 0, 123);
   suppress_one_message ();
   mock_rs_hangs_up (request);
   assert (! future_get_bool (future));
   request_destroy (request);

   future_destroy (future);
   future = future_cursor_destroy (cursor);

   /* driver does not reconnect just to send killcursors */
   mock_rs_set_request_timeout_msec (rs, 100);
   assert (! mock_rs_receives_kill_cursors (rs, 123));

   future_wait (future);
   future_destroy (future);
   mongoc_read_prefs_destroy (prefs);
   mongoc_collection_destroy (collection);
   bson_destroy (q);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   mock_rs_destroy (rs);
}


static void
test_getmore_fail_with_primary_single (void)
{
   _test_getmore_fail (true, false);
}


static void
test_getmore_fail_with_primary_pooled (void)
{
   _test_getmore_fail (true, true);
}


static void
test_getmore_fail_no_primary_pooled (void)
{
   _test_getmore_fail (false, true);
}


static void
test_getmore_fail_no_primary_single (void)
{
   _test_getmore_fail (false, false);
}


/* We already test that mongoc_cursor_destroy sends OP_KILLCURSORS in
 * test_kill_cursors_single / pooled. Here, test explicit
 * mongoc_client_kill_cursor. */
static void
_test_client_kill_cursor (bool has_primary)
{
   mock_rs_t *rs;
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_read_prefs_t *read_prefs;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   future_t *future;
   request_t *request;

   /* maybe a primary, definitely a secondary and no arbiter */
   rs = mock_rs_with_autoismaster (0, has_primary, 1, 0);
   mock_rs_run (rs);
   client = mongoc_client_new_from_uri (mock_rs_get_uri (rs));
   collection = mongoc_client_get_collection (client, "test", "test");
   read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   cursor = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0,
                                    0, tmp_bson ("{}"), NULL, read_prefs);

   future = future_cursor_next (cursor, &doc);

   request = mock_rs_receives_query (rs, "test.test", MONGOC_QUERY_SLAVE_OK,
                                     0, 0, "{}", NULL);

   assert (mock_rs_request_is_to_secondary (rs, request));

   mock_rs_replies (request,
                    0,                    /* flags */
                    123,                  /* cursorId */
                    0,                    /* startingFrom */
                    1,                    /* numberReturned */
                    "{'a': 1}");

   assert (future_get_bool (future));  /* mongoc_cursor_next returned true */
   future_destroy (future);
   request_destroy (request);

   future = future_client_kill_cursor (client, 123);

   mock_rs_set_request_timeout_msec (rs, 100);
   request = mock_rs_receives_kill_cursors (rs, 123);

   if (has_primary) {
      assert (request);

      /* weird but true. see mongoc_client_kill_cursor's documentation */
      assert (mock_rs_request_is_to_primary (rs, request));

      request_destroy (request);  /* server has no reply to OP_KILLCURSORS */
   } else {
      /* TODO: catch and check warning */
      assert (!request);
   }

   future_wait (future);  /* no return value */

   future_destroy (future);
   mongoc_cursor_destroy (cursor);
   mongoc_read_prefs_destroy (read_prefs);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   mock_rs_destroy (rs);
}

static void
test_client_kill_cursor_with_primary (void)
{
   _test_client_kill_cursor (true);
}


static void
test_client_kill_cursor_without_primary (void)
{
   _test_client_kill_cursor (false);
}


void
test_cursor_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Cursor/get_host", test_get_host);
   TestSuite_Add (suite, "/Cursor/clone", test_clone);
   TestSuite_Add (suite, "/Cursor/invalid_query", test_invalid_query);
   TestSuite_Add (suite, "/Cursor/kill/single", test_kill_cursors_single);
   TestSuite_Add (suite, "/Cursor/kill/pooled", test_kill_cursors_pooled);
   TestSuite_Add (suite, "/Cursor/getmore_fail/with_primary/pooled",
                  test_getmore_fail_with_primary_pooled);
   TestSuite_Add (suite, "/Cursor/getmore_fail/with_primary/single",
                  test_getmore_fail_with_primary_single);
   TestSuite_Add (suite, "/Cursor/getmore_fail/no_primary/pooled",
                  test_getmore_fail_no_primary_pooled);
   TestSuite_Add (suite, "/Cursor/getmore_fail/no_primary/single",
                  test_getmore_fail_no_primary_single);
   TestSuite_Add (suite, "/Cursor/client_kill_cursor/with_primary",
                  test_client_kill_cursor_with_primary);
   TestSuite_Add (suite, "/Cursor/client_kill_cursor/without_primary",
                  test_client_kill_cursor_without_primary);
}
