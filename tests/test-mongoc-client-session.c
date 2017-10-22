#include "mongoc.h"
#include "mongoc-util-private.h"
#include "mongoc-collection-private.h"
#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "session-test"


static void
test_session_opts_clone (void)
{
   mongoc_session_opt_t *opts;
   mongoc_session_opt_t *clone;

   opts = mongoc_session_opts_new ();
   clone = mongoc_session_opts_clone (opts);
   BSON_ASSERT (!mongoc_session_opts_get_causal_consistency (clone));
   mongoc_session_opts_destroy (clone);

   mongoc_session_opts_set_causal_consistency (opts, true);
   clone = mongoc_session_opts_clone (opts);
   BSON_ASSERT (mongoc_session_opts_get_causal_consistency (clone));
   mongoc_session_opts_destroy (clone);

   mongoc_session_opts_destroy (opts);
}


static void
test_session_no_crypto (void *ctx)
{
   mongoc_client_t *client;
   bson_error_t error;

   client = test_framework_client_new ();
   BSON_ASSERT (!mongoc_client_start_session (client, NULL, &error));
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_CLIENT,
                          MONGOC_ERROR_CLIENT_SESSION_FAILURE,
                          "need a cryptography library");

   mongoc_client_destroy (client);
}


#define ASSERT_SESSIONS_MATCH(_lsid_a, _lsid_b) \
   do {                                         \
      match_bson ((_lsid_a), (_lsid_b), false); \
   } while (0)


#define ASSERT_SESSIONS_DIFFER(_lsid_a, _lsid_b)                              \
   do {                                                                       \
      /* need a match context when checking that lsids DON'T match */         \
      char errmsg[1000];                                                      \
      match_ctx_t ctx = {0};                                                  \
      ctx.errmsg = errmsg;                                                    \
      ctx.errmsg_len = sizeof (errmsg);                                       \
      BSON_ASSERT (!match_bson_with_ctx ((_lsid_a), (_lsid_b), false, &ctx)); \
   } while (0)


/* "Pool is LIFO" test from Driver Sessions Spec */
static void
_test_session_pool_lifo (bool pooled)
{
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   mongoc_client_session_t *a, *b, *c, *d;
   bson_t lsid_a, lsid_b;
   bson_error_t error;

   if (pooled) {
      pool = test_framework_client_pool_new ();
      client = mongoc_client_pool_pop (pool);
   } else {
      client = test_framework_client_new ();
   }

   a = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (a, error);
   a->server_session->last_used_usec = bson_get_monotonic_time ();
   bson_copy_to (mongoc_client_session_get_lsid (a), &lsid_a);

   b = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (b, error);
   b->server_session->last_used_usec = bson_get_monotonic_time ();
   bson_copy_to (mongoc_client_session_get_lsid (b), &lsid_b);

   /* return server sessions to pool: first "a", then "b" */
   mongoc_client_session_destroy (a);
   mongoc_client_session_destroy (b);

   /* first pop returns last push */
   c = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (c, error);
   ASSERT_SESSIONS_MATCH (&lsid_b, mongoc_client_session_get_lsid (c));

   /* second pop returns previous push */
   d = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (d, error);
   ASSERT_SESSIONS_MATCH (&lsid_a, mongoc_client_session_get_lsid (d));

   mongoc_client_session_destroy (c);
   mongoc_client_session_destroy (d);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   bson_destroy (&lsid_a);
   bson_destroy (&lsid_b);
}


static void
test_session_pool_lifo_single (void *ctx)
{
   _test_session_pool_lifo (false);
}


static void
test_session_pool_lifo_pooled (void *ctx)
{
   _test_session_pool_lifo (true);
}


/* test that a session that is timed out is not added to the pool,
 * and a session that times out while it's in the pool is destroyed
 */
static void
_test_session_pool_timeout (bool pooled)
{
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   mongoc_client_session_t *s;
   bool r;
   bson_error_t error;
   bson_t lsid;
   int64_t almost_timeout_usec;

   almost_timeout_usec =
      (test_framework_session_timeout_minutes () - 1) * 60 * 1000 * 1000;

   if (pooled) {
      pool = test_framework_client_pool_new ();
      client = mongoc_client_pool_pop (pool);
   } else {
      client = test_framework_client_new ();
   }

   /*
    * trigger discovery
    */
   r = mongoc_client_command_simple (
      client, "admin", tmp_bson ("{'ping': 1}"), NULL, NULL, &error);
   ASSERT_OR_PRINT (r, error);

   /*
    * get a session, set last_used_date more than 29 minutes ago and return to
    * the pool. it's timed out & freed.
    */
   BSON_ASSERT (!client->topology->session_pool);
   s = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (s, error);
   bson_copy_to (mongoc_client_session_get_lsid (s), &lsid);

   s->server_session->last_used_usec =
      (bson_get_monotonic_time () - almost_timeout_usec - 100);

   mongoc_client_session_destroy (s);
   BSON_ASSERT (!client->topology->session_pool);

   /*
    * get a new session, set last_used_date so it has one second left to live,
    * return to the pool, wait 1.5 seconds. it's timed out & freed.
    */
   s = mongoc_client_start_session (client, NULL, &error);
   ASSERT_SESSIONS_DIFFER (&lsid, mongoc_client_session_get_lsid (s));

   bson_copy_to (mongoc_client_session_get_lsid (s), &lsid);

   s->server_session->last_used_usec =
      (bson_get_monotonic_time () + 1000 * 1000 - almost_timeout_usec);

   mongoc_client_session_destroy (s);
   BSON_ASSERT (client->topology->session_pool);
   ASSERT_SESSIONS_MATCH (&lsid, &client->topology->session_pool->lsid);

   _mongoc_usleep (1500 * 1000);

   /* getting a new client session must start a new server session */
   s = mongoc_client_start_session (client, NULL, &error);
   ASSERT_SESSIONS_DIFFER (&lsid, mongoc_client_session_get_lsid (s));
   BSON_ASSERT (!client->topology->session_pool);
   mongoc_client_session_destroy (s);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   bson_destroy (&lsid);
}


static void
test_session_pool_timeout_single (void *ctx)
{
   _test_session_pool_timeout (false);
}


static void
test_session_pool_timeout_pooled (void *ctx)
{
   _test_session_pool_timeout (true);
}


/* test that a session that times out while it's in the pool is reaped when
 * another session is added
 */
static void
_test_session_pool_reap (bool pooled)
{
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   mongoc_client_session_t *a, *b;
   bool r;
   bson_error_t error;
   bson_t lsid_a, lsid_b;
   int64_t almost_timeout_usec;
   mongoc_server_session_t *session_pool;

   almost_timeout_usec =
      (test_framework_session_timeout_minutes () - 1) * 60 * 1000 * 1000;

   if (pooled) {
      pool = test_framework_client_pool_new ();
      client = mongoc_client_pool_pop (pool);
   } else {
      client = test_framework_client_new ();
   }

   /*
    * trigger discovery
    */
   r = mongoc_client_command_simple (
      client, "admin", tmp_bson ("{'ping': 1}"), NULL, NULL, &error);
   ASSERT_OR_PRINT (r, error);

   /*
    * get a new session, set last_used_date so it has one second left to live,
    * return to the pool, wait 1.5 seconds.
    */
   a = mongoc_client_start_session (client, NULL, &error);
   b = mongoc_client_start_session (client, NULL, &error);
   bson_copy_to (mongoc_client_session_get_lsid (a), &lsid_a);
   bson_copy_to (mongoc_client_session_get_lsid (b), &lsid_b);

   a->server_session->last_used_usec =
      (bson_get_monotonic_time () + 1000 * 1000 - almost_timeout_usec);

   mongoc_client_session_destroy (a);
   BSON_ASSERT (client->topology->session_pool); /* session is pooled */

   _mongoc_usleep (1500 * 1000);

   /*
    * returning session B causes session A to be reaped
    */
   b->server_session->last_used_usec = bson_get_monotonic_time ();
   mongoc_client_session_destroy (b);
   BSON_ASSERT (client->topology->session_pool);
   ASSERT_SESSIONS_MATCH (&lsid_b, &client->topology->session_pool->lsid);
   /* session B is the only session in the pool */
   session_pool = client->topology->session_pool;
   BSON_ASSERT (session_pool == session_pool->prev);
   BSON_ASSERT (session_pool == session_pool->next);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   bson_destroy (&lsid_a);
}


static void
test_session_pool_reap_single (void *ctx)
{
   _test_session_pool_reap (false);
}


static void
test_session_pool_reap_pooled (void *ctx)
{
   _test_session_pool_reap (true);
}


static void
test_session_id_bad (void *ctx)
{
   const char *bad_opts[] = {
      "{'sessionId': null}",
      "{'sessionId': 'foo'}",
      "{'sessionId': {'$numberInt': '1'}}",
      "{'sessionId': {'$numberDouble': '1'}}",
      /* doesn't fit in uint32 */
      "{'sessionId': {'$numberLong': '5000000000'}}",
      /* doesn't match existing mongoc_client_session_t */
      "{'sessionId': {'$numberLong': '123'}}",
      NULL,
   };

   const char **bad_opt;
   mongoc_client_t *client;
   bson_error_t error;
   bool r;

   client = test_framework_client_new ();
   for (bad_opt = bad_opts; *bad_opt; bad_opt++) {
      r = mongoc_client_read_command_with_opts (client,
                                                "admin",
                                                tmp_bson ("{'ping': 1}"),
                                                NULL,
                                                tmp_bson (*bad_opt),
                                                NULL,
                                                &error);

      BSON_ASSERT (!r);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_COMMAND,
                             MONGOC_ERROR_COMMAND_INVALID_ARG,
                             "Invalid sessionId");

      memset (&error, 0, sizeof (bson_error_t));
   }

   mongoc_client_destroy (client);
}


typedef struct {
   mongoc_client_t *session_client, *client;
   mongoc_database_t *session_db, *db;
   mongoc_collection_t *session_collection, *collection;
   mongoc_client_session_t *cs;
   bson_t opts;
   bson_error_t error;
   int n_started;
   bool monitor;
   bool succeeded;
} session_test_t;


static void
started (const mongoc_apm_command_started_t *event)
{
   bson_iter_t iter;
   bson_t lsid;
   const bson_t *cmd = mongoc_apm_command_started_get_command (event);
   const char *cmd_name = mongoc_apm_command_started_get_command_name (event);
   session_test_t *test =
      (session_test_t *) mongoc_apm_command_started_get_context (event);

   if (!test->monitor) {
      return;
   }

   if (strcmp (cmd_name, "killCursors") == 0) {
      /* we omit lsid from killCursors, as permitted by Driver Sessions Spec */
      return;
   }

   test->n_started++;

   if (!bson_iter_init_find (&iter, cmd, "lsid")) {
      fprintf (stderr, "no lsid sent with command %s\n", cmd_name);
      abort ();
   }

   bson_iter_bson (&iter, &lsid);
   match_bson (&lsid, &test->cs->server_session->lsid, false);
}


static session_test_t *
session_test_new (bool correct_client)
{
   mongoc_apm_callbacks_t *callbacks;
   session_test_t *test;
   bson_error_t error;

   test = bson_malloc0 (sizeof (session_test_t));

   test->n_started = 0;
   test->monitor = true;
   test->succeeded = false;

   test->session_client = test_framework_client_new ();
   mongoc_client_set_error_api (test->session_client, 2);
   test->session_db = mongoc_client_get_database (test->session_client, "db");
   test->session_collection =
      mongoc_database_get_collection (test->session_db, "collection");

   bson_init (&test->opts);

   if (correct_client) {
      test->client = test->session_client;
      test->db = test->session_db;
      test->collection = test->session_collection;
   } else {
      test->client = test_framework_client_new ();
      mongoc_client_set_error_api (test->client, 2);
      test->db = mongoc_client_get_database (test->client, "db");
      test->collection =
         mongoc_database_get_collection (test->db, "collection");
   }

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, started);
   mongoc_client_set_apm_callbacks (test->client, callbacks, test);

   /* test each function with a session from the correct client and a session
    * from the wrong client */
   test->cs = mongoc_client_start_session (test->session_client, NULL, &error);
   ASSERT_OR_PRINT (test->cs, error);
   ASSERT_OR_PRINT (
      mongoc_client_session_append (test->cs, &test->opts, &error), error);

   mongoc_apm_callbacks_destroy (callbacks);

   return test;
}


static void
session_test_destroy (session_test_t *test)
{
   if (test->client != test->session_client) {
      mongoc_collection_destroy (test->collection);
      mongoc_database_destroy (test->db);
      mongoc_client_destroy (test->client);
   }

   mongoc_client_session_destroy (test->cs);
   mongoc_collection_destroy (test->session_collection);
   mongoc_database_destroy (test->session_db);
   mongoc_client_destroy (test->session_client);
   bson_destroy (&test->opts);
   bson_free (test);
}


typedef void (*session_test_fn_t) (session_test_t *);


static void
run_session_test (void *ctx)
{
   session_test_fn_t test_fn = (session_test_fn_t) ctx;
   session_test_t *test;
   bson_error_t error;
   bson_t opts = BSON_INITIALIZER;

   /*
    * use the same client for the session and the operation, expect success
    */
   test = session_test_new (true);
   test_fn (test);
   ASSERT_OR_PRINT (test->succeeded, test->error);
   ASSERT_CMPINT (test->n_started, >, 0);
   session_test_destroy (test);

   /*
    * use a session from the wrong client, expect failure
    */
   test = session_test_new (false);
   test_fn (test);
   BSON_ASSERT (!test->succeeded);
   error = test->error;
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_COMMAND,
                          MONGOC_ERROR_COMMAND_INVALID_ARG,
                          "Invalid sessionId");

   mongoc_client_session_append (test->cs, &opts, NULL);
   mongoc_collection_drop_with_opts (test->session_collection, &opts, NULL);
   session_test_destroy (test);
   bson_destroy (&opts);
}


/* "session argument is for right client" tests from Driver Sessions Spec */
static void
test_session_read_cmd (session_test_t *test)
{
   test->succeeded =
      mongoc_client_read_command_with_opts (test->client,
                                            "db",
                                            tmp_bson ("{'ping': 1}"),
                                            NULL,
                                            &test->opts,
                                            NULL,
                                            &test->error);
}


static void
test_session_count (session_test_t *test)
{
   test->succeeded =
      (-1 != mongoc_collection_count_with_opts (test->collection,
                                                MONGOC_QUERY_NONE,
                                                NULL,
                                                0,
                                                0,
                                                &test->opts,
                                                NULL,
                                                &test->error));
}


static void
test_session_cursor (session_test_t *test)
{
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   cursor = mongoc_collection_find_with_opts (
      test->collection, tmp_bson ("{}"), &test->opts, NULL);

   BSON_ASSERT (!mongoc_cursor_next (cursor, &doc));
   test->succeeded = !mongoc_cursor_error (cursor, &test->error);

   mongoc_cursor_destroy (cursor);
}


static void
test_session_drop (session_test_t *test)
{
   bson_error_t error;
   bool r;
   bson_t opts = BSON_INITIALIZER;

   mongoc_client_session_append (test->cs, &opts, NULL);

   /* create the collection so that "drop" can succeed */
   r = mongoc_database_write_command_with_opts (
      test->session_db,
      tmp_bson ("{'create': 'collection'}"),
      &opts,
      NULL,
      &error);

   ASSERT_OR_PRINT (r, error);
   test->succeeded = mongoc_collection_drop_with_opts (
      test->collection, &test->opts, &test->error);

   bson_destroy (&opts);
}


static void
test_session_drop_index (session_test_t *test)
{
   bson_error_t error;
   bool r;

   /* create the index so that "dropIndexes" can succeed */
   r = mongoc_database_write_command_with_opts (
      test->session_db,
      tmp_bson ("{'createIndexes': '%s',"
                " 'indexes': [{'key': {'a': 1}, 'name': 'foo'}]}",
                test->collection->collection),
      &test->opts,
      NULL,
      &error);

   ASSERT_OR_PRINT (r, error);

   test->succeeded = mongoc_collection_drop_index_with_opts (
      test->collection, "foo", &test->opts, &test->error);
}

static void
test_session_create_index (session_test_t *test)
{
   BEGIN_IGNORE_DEPRECATIONS
   test->succeeded =
      mongoc_collection_create_index_with_opts (test->collection,
                                                tmp_bson ("{'a': 1}"),
                                                NULL,
                                                &test->opts,
                                                NULL,
                                                &test->error);
   END_IGNORE_DEPRECATIONS
}

static void
test_session_replace_one (session_test_t *test)
{
   test->succeeded = mongoc_collection_replace_one_with_opts (test->collection,
                                                              tmp_bson ("{}"),
                                                              tmp_bson ("{}"),
                                                              &test->opts,
                                                              NULL,
                                                              &test->error);
}

static void
test_session_rename (session_test_t *test)
{
   bson_error_t error;
   bool r;

   /* ensure "rename" can succeed */
   mongoc_database_write_command_with_opts (test->session_db,
                                            tmp_bson ("{'drop': 'newname'}"),
                                            &test->opts,
                                            NULL,
                                            NULL);

   r = mongoc_database_write_command_with_opts (
      test->session_db,
      tmp_bson ("{'insert': 'collection', 'documents': [{}]}"),
      &test->opts,
      NULL,
      NULL);

   ASSERT_OR_PRINT (r, error);
   test->succeeded = mongoc_collection_rename_with_opts (
      test->collection, "db", "newname", true, &test->opts, &test->error);
}

static void
test_session_fam (session_test_t *test)
{
   mongoc_find_and_modify_opts_t *fam_opts;

   fam_opts = mongoc_find_and_modify_opts_new ();
   mongoc_find_and_modify_opts_set_update (fam_opts,
                                           tmp_bson ("{'$set': {'x': 1}}"));
   BSON_ASSERT (mongoc_find_and_modify_opts_append (fam_opts, &test->opts));
   test->succeeded = mongoc_collection_find_and_modify_with_opts (
      test->collection, tmp_bson ("{}"), fam_opts, NULL, &test->error);

   mongoc_find_and_modify_opts_destroy (fam_opts);
}

static void
test_session_db_drop (session_test_t *test)
{
   test->succeeded =
      mongoc_database_drop_with_opts (test->db, &test->opts, &test->error);
}

static void
test_session_gridfs_find (session_test_t *test)
{
   mongoc_gridfs_t *gfs;
   bson_error_t error;
   mongoc_gridfs_file_list_t *list;
   mongoc_gridfs_file_t *f;

   /* work around lack of mongoc_client_get_gridfs_with_opts for now, can't yet
    * include lsid with the GridFS createIndexes command */
   test->monitor = false;
   gfs = mongoc_client_get_gridfs (test->client, "test", NULL, &error);
   ASSERT_OR_PRINT (gfs, error);
   test->monitor = true;
   list = mongoc_gridfs_find_with_opts (gfs, tmp_bson ("{}"), &test->opts);
   f = mongoc_gridfs_file_list_next (list);
   test->succeeded = !mongoc_gridfs_file_list_error (list, &test->error);

   if (f) {
      mongoc_gridfs_file_destroy (f);
   }

   mongoc_gridfs_file_list_destroy (list);
   mongoc_gridfs_destroy (gfs);
}

static void
test_session_gridfs_find_one (session_test_t *test)
{
   mongoc_gridfs_t *gfs;
   bson_error_t error;
   mongoc_gridfs_file_t *f;

   /* work around lack of mongoc_client_get_gridfs_with_opts for now, can't yet
    * include lsid with the GridFS createIndexes command */
   test->monitor = false;
   gfs = mongoc_client_get_gridfs (test->client, "test", NULL, &error);
   ASSERT_OR_PRINT (gfs, error);
   test->monitor = true;
   f = mongoc_gridfs_find_one_with_opts (
      gfs, tmp_bson ("{}"), &test->opts, &test->error);

   test->succeeded = test->error.domain == 0;

   if (f) {
      mongoc_gridfs_file_destroy (f);
   }

   mongoc_gridfs_destroy (gfs);
}


static void
test_watch (session_test_t *test)
{
   mongoc_change_stream_t *change_stream;

   change_stream =
      mongoc_collection_watch (test->collection, tmp_bson ("{}"), &test->opts);

   test->succeeded =
      !mongoc_change_stream_error_document (change_stream, &test->error, NULL);
   mongoc_change_stream_destroy (change_stream);
}


static void
test_aggregate (session_test_t *test)
{
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   cursor = mongoc_collection_aggregate (
      test->collection, MONGOC_QUERY_NONE, tmp_bson ("{}"), &test->opts, NULL);

   mongoc_cursor_next (cursor, &doc);
   test->succeeded = !mongoc_cursor_error (cursor, &test->error);
   mongoc_cursor_destroy (cursor);
}


static void
test_create (session_test_t *test)
{
   mongoc_collection_t *collection;

   /* ensure "create" can succeed */
   mongoc_database_write_command_with_opts (test->session_db,
                                            tmp_bson ("{'drop': 'newname'}"),
                                            &test->opts,
                                            NULL,
                                            NULL);

   collection = mongoc_database_create_collection (
      test->db, "newname", &test->opts, &test->error);

   test->succeeded = (collection != NULL);

   if (collection) {
      mongoc_collection_destroy (collection);
   }
}


static void
test_database_names (session_test_t *test)
{
   char **names;

   names = mongoc_client_get_database_names_with_opts (
      test->client, &test->opts, &test->error);

   test->succeeded = (names != NULL);

   if (names) {
      bson_strfreev (names);
   }
}


static void
add_session_test (TestSuite *suite, const char *name, session_test_fn_t test_fn)
{
   TestSuite_AddFull (suite,
                      name,
                      run_session_test,
                      NULL,
                      (void *) test_fn,
                      test_framework_skip_if_no_sessions,
                      test_framework_skip_if_no_crypto);
}


void
test_session_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Session/opts/clone", test_session_opts_clone);
   TestSuite_AddFull (suite,
                      "/Session/no_crypto",
                      test_session_no_crypto,
                      NULL,
                      NULL,
                      TestSuite_CheckLive,
                      test_framework_skip_if_crypto);
   TestSuite_AddFull (suite,
                      "/Session/lifo/single",
                      test_session_pool_lifo_single,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_sessions,
                      test_framework_skip_if_no_crypto);
   TestSuite_AddFull (suite,
                      "/Session/lifo/pooled",
                      test_session_pool_lifo_pooled,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_sessions,
                      test_framework_skip_if_no_crypto);
   TestSuite_AddFull (suite,
                      "/Session/timeout/single",
                      test_session_pool_timeout_single,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_sessions,
                      test_framework_skip_if_no_crypto,
                      test_framework_skip_if_slow);
   TestSuite_AddFull (suite,
                      "/Session/timeout/pooled",
                      test_session_pool_timeout_pooled,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_sessions,
                      test_framework_skip_if_no_crypto,
                      test_framework_skip_if_slow);
   TestSuite_AddFull (suite,
                      "/Session/reap/single",
                      test_session_pool_reap_single,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_sessions,
                      test_framework_skip_if_no_crypto,
                      test_framework_skip_if_slow);
   TestSuite_AddFull (suite,
                      "/Session/reap/pooled",
                      test_session_pool_reap_pooled,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_sessions,
                      test_framework_skip_if_no_crypto,
                      test_framework_skip_if_slow);
   TestSuite_AddFull (suite,
                      "/Session/id_bad",
                      test_session_id_bad,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_sessions,
                      test_framework_skip_if_no_crypto);
   add_session_test (suite, "/Session/read_cmd", test_session_read_cmd);
   add_session_test (suite, "/Session/count", test_session_count);
   add_session_test (suite, "/Session/cursor", test_session_cursor);
   add_session_test (suite, "/Session/drop", test_session_drop);
   add_session_test (suite, "/Session/drop_index", test_session_drop_index);
   add_session_test (suite, "/Session/create_index", test_session_create_index);
   add_session_test (suite, "/Session/replace_one", test_session_replace_one);
   add_session_test (suite, "/Session/rename", test_session_rename);
   add_session_test (suite, "/Session/fam", test_session_fam);
   add_session_test (suite, "/Session/db_drop", test_session_db_drop);
   add_session_test (suite, "/Session/gridfs_find", test_session_gridfs_find);
   add_session_test (
      suite, "/Session/gridfs_find_one", test_session_gridfs_find_one);
   add_session_test_wc (suite,
                        "/Session/watch",
                        test_watch,
                        test_framework_skip_if_not_rs_version_6);
   add_session_test (suite, "/Session/aggregate", test_aggregate);
   add_session_test (suite, "/Session/create", test_create);
   add_session_test (suite, "/Session/database_names", test_database_names);
}
