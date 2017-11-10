#include "mongoc.h"
#include "mongoc-util-private.h"
#include "mongoc-collection-private.h"
#include "utlist.h"
#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include "mock_server/mock-server.h"
#include "mock_server/future-functions.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "session-test"


static void
test_session_opts_clone (void)
{
   mongoc_session_opt_t *opts;
   mongoc_session_opt_t *clone;

   opts = mongoc_session_opts_new ();
   clone = mongoc_session_opts_clone (opts);
   /* causal is enabled by default */
   BSON_ASSERT (mongoc_session_opts_get_causal_consistency (clone));
   mongoc_session_opts_destroy (clone);

   mongoc_session_opts_set_causal_consistency (opts, false);
   clone = mongoc_session_opts_clone (opts);
   BSON_ASSERT (!mongoc_session_opts_get_causal_consistency (clone));

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
      /* the pooled client never needed to connect, so it warns that
       * it isn't connecting in order to send endSessions */
      capture_logs (true);
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
   uint32_t server_id;
   mongoc_client_session_t *s;
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
   server_id = mongoc_topology_select_server_id (
      client->topology, MONGOC_SS_READ, NULL, &error);
   ASSERT_OR_PRINT (server_id, error);

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
      /* the pooled client never needed to connect, so it warns that
       * it isn't connecting in order to send endSessions */
      capture_logs (true);
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

static void
_test_session_supported (bool pooled)
{
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_client_session_t *session;

   if (pooled) {
      pool = test_framework_client_pool_new ();
      client = mongoc_client_pool_pop (pool);
   } else {
      client = test_framework_client_new ();
   }

   if (test_framework_session_timeout_minutes () == -1) {
      BSON_ASSERT (!mongoc_client_start_session (client, NULL, &error));
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_SESSION_FAILURE,
                             "Server does not support sessions");
   } else {
      session = mongoc_client_start_session (client, NULL, &error);
      ASSERT_OR_PRINT (session, error);
      mongoc_client_session_destroy (session);
   }

   if (pooled) {
      /* the pooled client never needed to connect, so it warns that
       * it isn't connecting in order to send endSessions */
      capture_logs (true);
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }
}

static void
test_session_supported_single (void *ctx)
{
   _test_session_supported (false);
}

static void
test_session_supported_pooled (void *ctx)
{
   _test_session_supported (true);
}

static void
_test_mock_end_sessions (bool pooled)
{
   mock_server_t *server;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_client_session_t *session;
   bson_t lsid = BSON_INITIALIZER;
   bson_t opts = BSON_INITIALIZER;
   bson_t *expected_cmd;
   future_t *future;
   request_t *request;
   bool r;

   server = mock_mongos_new (WIRE_VERSION_OP_MSG);
   mock_server_run (server);

   if (pooled) {
      pool = mongoc_client_pool_new (mock_server_get_uri (server));
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (mock_server_get_uri (server));
   }

   session = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (session, error);
   bson_copy_to (mongoc_client_session_get_lsid (session), &lsid);
   r = mongoc_client_session_append (session, &opts, &error);
   ASSERT_OR_PRINT (r, error);

   future = future_client_command_with_opts (
      client, "admin", tmp_bson ("{'ping': 1}"), NULL, &opts, NULL, &error);

   request = mock_server_receives_msg (
      server, 0, tmp_bson ("{'ping': 1, 'lsid': {'$exists': true}}"));
   mock_server_replies_ok_and_destroys (request);

   BSON_ASSERT (future_get_bool (future));
   future_destroy (future);

   /* before destroying the session, construct the expected endSessions cmd */
   expected_cmd =
      BCON_NEW ("endSessions",
                "[",
                BCON_DOCUMENT (mongoc_client_session_get_lsid (session)),
                "]");

   mongoc_client_session_destroy (session);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      future = future_client_pool_destroy (pool);
   } else {
      future = future_client_destroy (client);
   }

   /* check that we got the expected endSessions cmd */
   request = mock_server_receives_msg (server, 0, expected_cmd);
   mock_server_replies_ok_and_destroys (request);
   future_wait (future);

   mock_server_destroy (server);
   bson_destroy (expected_cmd);
   bson_destroy (&lsid);
   bson_destroy (&opts);
}

static void
test_mock_end_sessions_single (void)
{
   _test_mock_end_sessions (false);
}

static void
test_mock_end_sessions_pooled (void)
{
   _test_mock_end_sessions (true);
}

typedef struct {
   int started_calls;
   int succeeded_calls;
   bson_t cmd;
} endsessions_test_t;

static void
endsessions_started_cb (const mongoc_apm_command_started_t *event)
{
   endsessions_test_t *test;

   if (strcmp (mongoc_apm_command_started_get_command_name (event),
               "endSessions") != 0) {
      return;
   }

   test = (endsessions_test_t *) mongoc_apm_command_started_get_context (event);
   test->started_calls++;
   bson_destroy (&test->cmd);
   bson_copy_to (mongoc_apm_command_started_get_command (event), &test->cmd);
}

static void
endsessions_succeeded_cb (const mongoc_apm_command_succeeded_t *event)
{
   endsessions_test_t *test;

   if (strcmp (mongoc_apm_command_succeeded_get_command_name (event),
               "endSessions") != 0) {
      return;
   }

   test =
      (endsessions_test_t *) mongoc_apm_command_succeeded_get_context (event);
   test->succeeded_calls++;
}

static void
_test_end_sessions (bool pooled)
{
   endsessions_test_t test;
   mongoc_apm_callbacks_t *callbacks;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_client_session_t *cs1;
   mongoc_client_session_t *cs2;
   bson_t lsid1 = BSON_INITIALIZER;
   bson_t lsid2 = BSON_INITIALIZER;
   bson_t opts1 = BSON_INITIALIZER;
   bson_t opts2 = BSON_INITIALIZER;
   bool lsid1_ended = false;
   bool lsid2_ended = false;
   bson_iter_t iter;
   bson_iter_t ended_lsids;
   bson_t ended_lsid;
   char errmsg[1000];
   match_ctx_t ctx = {0};
   bool r;

   bson_init (&test.cmd);
   test.started_calls = test.succeeded_calls = 0;
   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, endsessions_started_cb);
   mongoc_apm_set_command_succeeded_cb (callbacks, endsessions_succeeded_cb);

   if (pooled) {
      pool = test_framework_client_pool_new ();
      ASSERT (mongoc_client_pool_set_apm_callbacks (pool, callbacks, &test));
      client = mongoc_client_pool_pop (pool);
   } else {
      client = test_framework_client_new ();
      ASSERT (mongoc_client_set_apm_callbacks (client, callbacks, &test));
   }

   mongoc_apm_callbacks_destroy (callbacks);

   /*
    * create and use sessions 1 and 2
    */
   cs1 = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (cs1, error);
   bson_copy_to (mongoc_client_session_get_lsid (cs1), &lsid1);
   r = mongoc_client_session_append (cs1, &opts1, &error);
   ASSERT_OR_PRINT (r, error);
   r = mongoc_client_command_with_opts (
      client, "admin", tmp_bson ("{'count': 'c'}"), NULL, &opts1, NULL, &error);
   ASSERT_OR_PRINT (r, error);

   cs2 = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (cs2, error);
   bson_copy_to (mongoc_client_session_get_lsid (cs2), &lsid2);
   r = mongoc_client_session_append (cs2, &opts2, &error);
   ASSERT_OR_PRINT (r, error);
   r = mongoc_client_command_with_opts (
      client, "admin", tmp_bson ("{'count': 'c'}"), NULL, &opts2, NULL, &error);
   ASSERT_OR_PRINT (r, error);

   /*
    * return server sessions to the pool
    */
   mongoc_client_session_destroy (cs1);
   mongoc_client_session_destroy (cs2);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   /*
    * sessions were ended on server
    */
   ASSERT_CMPINT (test.started_calls, ==, 1);
   ASSERT_CMPINT (test.succeeded_calls, ==, 1);

   BSON_ASSERT (bson_iter_init_find (&iter, &test.cmd, "endSessions"));
   BSON_ASSERT (BSON_ITER_HOLDS_ARRAY (&iter));
   BSON_ASSERT (bson_iter_recurse (&iter, &ended_lsids));

   ctx.errmsg = errmsg;
   ctx.errmsg_len = sizeof errmsg;

   while (bson_iter_next (&ended_lsids)) {
      BSON_ASSERT (BSON_ITER_HOLDS_DOCUMENT (&ended_lsids));
      bson_iter_bson (&ended_lsids, &ended_lsid);
      if (match_bson_with_ctx (&ended_lsid, &lsid1, false, &ctx)) {
         lsid1_ended = true;
      } else if (match_bson_with_ctx (&ended_lsid, &lsid2, false, &ctx)) {
         lsid2_ended = true;
      }
   }

   BSON_ASSERT (lsid1_ended);
   BSON_ASSERT (lsid2_ended);

   bson_destroy (&test.cmd);
   bson_destroy (&lsid1);
   bson_destroy (&opts1);
   bson_destroy (&lsid2);
   bson_destroy (&opts2);
}

static void
test_end_sessions_single (void *ctx)
{
   _test_end_sessions (false);
}

static void
test_end_sessions_pooled (void *ctx)
{
   _test_end_sessions (true);
}

static void
_test_advance_cluster_time (mongoc_client_session_t *cs,
                            int new_timestamp,
                            int new_increment,
                            bool should_advance)
{
   bson_t *old_cluster_time;
   bson_t *new_cluster_time;

   old_cluster_time = bson_copy (mongoc_client_session_get_cluster_time (cs));
   new_cluster_time =
      tmp_bson ("{'clusterTime': {'$timestamp': {'t': %d, 'i': %d}}}",
                new_timestamp,
                new_increment);

   mongoc_client_session_advance_cluster_time (cs, new_cluster_time);

   if (should_advance) {
      BSON_ASSERT (match_bson (
         mongoc_client_session_get_cluster_time (cs), new_cluster_time, false));
   } else {
      BSON_ASSERT (match_bson (
         mongoc_client_session_get_cluster_time (cs), old_cluster_time, false));
   }

   bson_destroy (old_cluster_time);
}

static void
test_session_advance_cluster_time (void *ctx)
{
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_client_session_t *cs;

   client = test_framework_client_new ();
   cs = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (cs, error);
   BSON_ASSERT (!mongoc_client_session_get_cluster_time (cs));

   capture_logs (true);
   mongoc_client_session_advance_cluster_time (cs, tmp_bson ("{'foo': 1}"));
   ASSERT_CAPTURED_LOG ("mongoc_client_session_advance_cluster_time",
                        MONGOC_LOG_LEVEL_ERROR,
                        "Cannot parse cluster time");

   capture_logs (true);
   mongoc_client_session_advance_cluster_time (cs,
                                               tmp_bson ("{'clusterTime': 1}"));
   ASSERT_CAPTURED_LOG ("mongoc_client_session_advance_cluster_time",
                        MONGOC_LOG_LEVEL_ERROR,
                        "Cannot parse cluster time");

   mongoc_client_session_advance_cluster_time (
      cs, tmp_bson ("{'clusterTime': {'$timestamp': {'t': 1, 'i': 1}}}"));

   _test_advance_cluster_time (cs, 1, 0, false);
   _test_advance_cluster_time (cs, 2, 2, true);
   _test_advance_cluster_time (cs, 2, 1, false);
   _test_advance_cluster_time (cs, 3, 1, true);

   mongoc_client_session_destroy (cs);
   mongoc_client_destroy (client);
}


static void
_test_advance_operation_time (mongoc_client_session_t *cs,
                              uint32_t t,
                              uint32_t i,
                              bool should_advance)
{
   uint32_t old_t, old_i;
   uint32_t new_t, new_i;

   mongoc_client_session_get_operation_time (cs, &old_t, &old_i);
   mongoc_client_session_advance_operation_time (cs, t, i);
   mongoc_client_session_get_operation_time (cs, &new_t, &new_i);

   if (should_advance) {
      ASSERT_CMPUINT32 (new_t, ==, t);
      ASSERT_CMPUINT32 (new_i, ==, i);
   } else if (new_t == t && new_i == i) {
      fprintf (stderr,
               "Shouldn't have advanced from operationTime %" PRIu32
               ", %" PRIu32 " to %" PRIu32 ", %" PRIu32 "\n",
               old_t,
               old_i,
               t,
               i);
      abort ();
   }
}


static void
test_session_advance_operation_time (void *ctx)
{
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_client_session_t *cs;
   uint32_t t, i;

   client = test_framework_client_new ();
   cs = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (cs, error);
   mongoc_client_session_get_operation_time (cs, &t, &i);

   ASSERT_CMPUINT32 (t, ==, 0);
   ASSERT_CMPUINT32 (t, ==, 0);


   mongoc_client_session_advance_operation_time (cs, 1, 1);

   _test_advance_operation_time (cs, 1, 0, false);
   _test_advance_operation_time (cs, 2, 2, true);
   _test_advance_operation_time (cs, 2, 1, false);
   _test_advance_operation_time (cs, 3, 1, true);

   mongoc_client_session_destroy (cs);
   mongoc_client_destroy (client);
}


typedef struct {
   mongoc_client_t *session_client, *client;
   mongoc_database_t *session_db, *db;
   mongoc_collection_t *session_collection, *collection;
   mongoc_client_session_t *cs;
   mongoc_client_session_t *wrong_cs;
   bson_t opts;
   bson_error_t error;
   int n_started;
   int n_succeeded;
   bool expect_explicit_lsid;
   bool succeeded;
   bson_t sent_lsid;
   bson_t sent_cluster_time;
   bson_t received_cluster_time;
} session_test_t;


static void
started (const mongoc_apm_command_started_t *event)
{
   char errmsg[1000];
   match_ctx_t ctx = {errmsg, sizeof (errmsg), false /* strict numbers */};
   bson_iter_t iter;
   bson_t cluster_time;
   bson_t lsid;
   const bson_t *client_session_lsid;
   const bson_t *cmd = mongoc_apm_command_started_get_command (event);
   const char *cmd_name = mongoc_apm_command_started_get_command_name (event);
   session_test_t *test =
      (session_test_t *) mongoc_apm_command_started_get_context (event);

   if (!strcmp (cmd_name, "endSessions")) {
      BSON_ASSERT (!bson_has_field (cmd, "lsid"));
      return;
   }

   if (!bson_iter_init_find (&iter, cmd, "lsid")) {
      fprintf (stderr, "no lsid sent with command %s\n", cmd_name);
      abort ();
   }

   bson_iter_bson (&iter, &lsid);
   client_session_lsid = &test->cs->server_session->lsid;

   if (test->expect_explicit_lsid) {
      if (!match_bson_with_ctx (&lsid, client_session_lsid, false, &ctx)) {
         fprintf (stderr,
                  "command %s should have used client session's lsid\n",
                  cmd_name);
         abort ();
      }
   } else {
      if (match_bson_with_ctx (&lsid, client_session_lsid, false, &ctx)) {
         fprintf (stderr,
                  "command %s should not have used client session's lsid\n",
                  cmd_name);
         abort ();
      }
   }

   if (bson_empty (&test->sent_lsid)) {
      bson_destroy (&test->sent_lsid);
      bson_copy_to (&lsid, &test->sent_lsid);
   } else {
      if (!match_bson_with_ctx (&lsid, &test->sent_lsid, false, &ctx)) {
         fprintf (stderr,
                  "command %s used different lsid than previous command\n",
                  cmd_name);
         abort ();
      }
   }

   if (!bson_iter_init_find (&iter, cmd, "$clusterTime")) {
      fprintf (stderr, "no $clusterTime sent with command %s\n", cmd_name);
      abort ();
   }

   /* like $clusterTime: {clusterTime: <timestamp>} */
   bson_iter_bson (&iter, &cluster_time);
   bson_destroy (&test->sent_cluster_time);
   bson_copy_to (&cluster_time, &test->sent_cluster_time);

   test->n_started++;
}


static void
succeeded (const mongoc_apm_command_succeeded_t *event)
{
   bson_iter_t iter;
   bson_t cluster_time;
   const bson_t *reply = mongoc_apm_command_succeeded_get_reply (event);
   const char *cmd_name = mongoc_apm_command_succeeded_get_command_name (event);
   session_test_t *test =
      (session_test_t *) mongoc_apm_command_succeeded_get_context (event);

   if (!bson_iter_init_find (&iter, reply, "$clusterTime")) {
      fprintf (stderr, "no $clusterTime in reply to command %s\n", cmd_name);
      abort ();
   }

   if (strcmp (cmd_name, "endSessions") == 0) {
      return;
   }

   /* like $clusterTime: {clusterTime: <timestamp>} */
   bson_iter_bson (&iter, &cluster_time);
   bson_destroy (&test->received_cluster_time);
   bson_copy_to (&cluster_time, &test->received_cluster_time);

   test->n_succeeded++;
}


static void
set_session_test_callbacks (session_test_t *test)
{
   mongoc_apm_callbacks_t *callbacks;

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, started);
   mongoc_apm_set_command_succeeded_cb (callbacks, succeeded);
   mongoc_client_set_apm_callbacks (test->client, callbacks, test);

   mongoc_apm_callbacks_destroy (callbacks);
}


static session_test_t *
session_test_new (bool correct_client)
{
   session_test_t *test;
   bson_error_t error;

   test = bson_malloc0 (sizeof (session_test_t));

   test->n_started = 0;
   test->expect_explicit_lsid = true;
   test->succeeded = false;
   bson_init (&test->sent_cluster_time);
   bson_init (&test->received_cluster_time);
   bson_init (&test->sent_lsid);

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
      /* test each function with a session from the correct client and a session
       * from the wrong client */
      test->client = test_framework_client_new ();
      mongoc_client_set_error_api (test->client, 2);
      test->wrong_cs = mongoc_client_start_session (test->client, NULL, &error);
      ASSERT_OR_PRINT (test->wrong_cs, error);
      test->db = mongoc_client_get_database (test->client, "db");
      test->collection =
         mongoc_database_get_collection (test->db, "collection");
   }

   set_session_test_callbacks (test);

   test->cs = mongoc_client_start_session (test->session_client, NULL, &error);
   ASSERT_OR_PRINT (test->cs, error);

   return test;
}


static void
check_session_returned (session_test_t *test, const bson_t *lsid)
{
   char errmsg[1000];
   match_ctx_t ctx = {errmsg, sizeof (errmsg), false /* strict numbers */};
   mongoc_server_session_t *ss;
   bool found;

   found = false;
   CDL_FOREACH (test->session_client->topology->session_pool, ss)
   {
      if (match_bson_with_ctx (&ss->lsid, lsid, false, &ctx)) {
         found = true;
         break;
      }
   }

   if (!found) {
      fprintf (stderr,
               "server session %s not returned to pool\n",
               bson_as_json (lsid, NULL));
      abort ();
   }
}


static void
session_test_destroy (session_test_t *test)
{
   bson_t session_lsid;

   bson_copy_to (mongoc_client_session_get_lsid (test->cs), &session_lsid);

   mongoc_client_session_destroy (test->cs);

   check_session_returned (test, &session_lsid);
   bson_destroy (&session_lsid);

   /* for implicit sessions, ensure the implicit session was returned */
   check_session_returned (test, &test->sent_lsid);

   if (test->client != test->session_client) {
      mongoc_client_session_destroy (test->wrong_cs);
      mongoc_collection_destroy (test->collection);
      mongoc_database_destroy (test->db);
      mongoc_client_destroy (test->client);
   }

   mongoc_collection_destroy (test->session_collection);
   mongoc_database_destroy (test->session_db);
   mongoc_client_destroy (test->session_client);
   bson_destroy (&test->opts);
   bson_destroy (&test->sent_cluster_time);
   bson_destroy (&test->received_cluster_time);
   bson_destroy (&test->sent_lsid);

   bson_free (test);
}


static void
check_success (session_test_t *test)
{
   if (test->session_client != test->client) {
      BSON_ASSERT (!test->succeeded);
      ASSERT_ERROR_CONTAINS (test->error,
                             MONGOC_ERROR_COMMAND,
                             MONGOC_ERROR_COMMAND_INVALID_ARG,
                             "Invalid sessionId");

      return;
   }

   ASSERT_OR_PRINT (test->succeeded, test->error);
}


static void
check_cluster_time (session_test_t *test)
{
   const bson_t *session_time;

   session_time = mongoc_client_session_get_cluster_time (test->cs);
   BSON_ASSERT (session_time); /* should be set during handshake */

   /* fail if cluster_time_greater logs an error */
   capture_logs (true);
   if (_mongoc_cluster_time_greater (&test->received_cluster_time,
                                     session_time)) {
      fprintf (stderr, "client session's cluster time is outdated\n");
      abort ();
   }

   ASSERT_NO_CAPTURED_LOGS ("_mongoc_cluster_time_greater");
   capture_logs (false);
}


typedef void (*session_test_fn_t) (session_test_t *);


static void
run_session_test (void *ctx)
{
   session_test_fn_t test_fn = (session_test_fn_t) ctx;
   session_test_t *test;
   bson_error_t error;
   int64_t start;
   bson_t cluster_time;
   bool r;

   /*
    * use the same client for the session and the operation, expect success
    *
    */
   test = session_test_new (true);
   ASSERT_CMPINT64 (test->cs->server_session->last_used_usec, ==, (int64_t) -1);
   ASSERT_OR_PRINT (
      mongoc_client_session_append (test->cs, &test->opts, &error), error);

   start = bson_get_monotonic_time ();
   test_fn (test);
   ASSERT_CMPINT (test->n_started, >, 0);
   ASSERT_CMPINT (test->n_succeeded, >, 0);
   check_success (test);
   check_cluster_time (test);
   ASSERT_CMPINT64 (test->cs->server_session->last_used_usec, >=, start);

   /*
    * disable monitoring, advance server's time with a write, set session's
    * cluster time, enable monitoring, ensure new cluster time is sent
    */
   mongoc_client_set_apm_callbacks (test->session_client, NULL, NULL);
   r = mongoc_collection_insert_one (
      test->session_collection, tmp_bson ("{}"), NULL, NULL, &error);
   ASSERT_OR_PRINT (r, error);
   mongoc_collection_drop_with_opts (test->session_collection, NULL, NULL);
   bson_copy_to (&test->client->topology->description.cluster_time,
                 &cluster_time);
   BSON_ASSERT (_mongoc_cluster_time_greater (
      &cluster_time, mongoc_client_session_get_cluster_time (test->cs)));

   capture_logs (true);
   mongoc_client_session_advance_cluster_time (test->cs, &cluster_time);
   ASSERT_NO_CAPTURED_LOGS ("_mongoc_cluster_time_greater");
   capture_logs (false);
   /* successfully set, not yet sent to server */
   match_bson (
      &cluster_time, mongoc_client_session_get_cluster_time (test->cs), false);

   set_session_test_callbacks (test);
   test->n_started = 0;
   test->n_succeeded = 0;
   start = bson_get_monotonic_time ();
   test_fn (test);
   ASSERT_CMPINT (test->n_started, >, 0);
   ASSERT_CMPINT (test->n_succeeded, >, 0);
   check_success (test);
   if (_mongoc_cluster_time_greater (&cluster_time, &test->sent_cluster_time)) {
      fprintf (stderr,
               "mongoc_client_session_advance_cluster_time didn't advance"
               " the cluster time sent with command\n");
      abort ();
   }

   check_cluster_time (test);
   ASSERT_CMPINT64 (test->cs->server_session->last_used_usec, >=, start);
   session_test_destroy (test);

   /*
    * use a session from the wrong client, expect failure. this is the
    * "session argument is for right client" test from Driver Sessions Spec
    */
   test = session_test_new (false /* correct_client */);
   ASSERT_OR_PRINT (
      mongoc_client_session_append (test->cs, &test->opts, &error), error);

   test_fn (test);
   check_success (test);
   mongoc_collection_drop_with_opts (test->session_collection, NULL, NULL);
   session_test_destroy (test);

   /*
    * implicit session - all commands should use an internally-acquired lsid
    */
   test = session_test_new (true /* correct_client */);
   test->expect_explicit_lsid = false;
   start = bson_get_monotonic_time ();
   test_fn (test);
   check_success (test);
   mongoc_collection_drop_with_opts (test->session_collection, NULL, NULL);
   BSON_ASSERT (test->client->topology->session_pool);
   ASSERT_CMPINT64 (
      test->client->topology->session_pool->last_used_usec, >=, start);
   session_test_destroy (test);
   bson_destroy (&cluster_time);
}


static void
insert_10_docs (session_test_t *test)
{
   mongoc_bulk_operation_t *bulk;
   bson_error_t error;
   int i;
   bool r;

   bulk = mongoc_collection_create_bulk_operation_with_opts (
      test->session_collection, &test->opts);

   for (i = 0; i < 10; i++) {
      mongoc_bulk_operation_insert (bulk, tmp_bson ("{}"));
   }

   r = (bool) mongoc_bulk_operation_execute (bulk, NULL, &error);
   ASSERT_OR_PRINT (r, error);

   mongoc_bulk_operation_destroy (bulk);
}


static void
test_cmd (session_test_t *test)
{
   test->succeeded = mongoc_client_command_with_opts (test->client,
                                                      "db",
                                                      tmp_bson ("{'ping': 1}"),
                                                      NULL,
                                                      &test->opts,
                                                      NULL,
                                                      &test->error);
}


static void
test_read_cmd (session_test_t *test)
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
test_db_cmd (session_test_t *test)
{
   test->succeeded =
      mongoc_database_command_with_opts (test->db,
                                         tmp_bson ("{'ping': 1}"),
                                         NULL,
                                         &test->opts,
                                         NULL,
                                         &test->error);
}


static void
test_count (session_test_t *test)
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
test_cursor (session_test_t *test)
{
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   /* ensure multiple batches */
   insert_10_docs (test);

   cursor = mongoc_collection_find_with_opts (
      test->collection, tmp_bson ("{}"), &test->opts, NULL);

   mongoc_cursor_set_batch_size (cursor, 2);
   while (mongoc_cursor_next (cursor, &doc)) {
   }

   test->succeeded = !mongoc_cursor_error (cursor, &test->error);

   mongoc_cursor_destroy (cursor);
}


static void
test_drop (session_test_t *test)
{
   /* create the collection so that "drop" can succeed */
   insert_10_docs (test);

   test->succeeded = mongoc_collection_drop_with_opts (
      test->collection, &test->opts, &test->error);
}


static void
test_drop_index (session_test_t *test)
{
   bson_error_t error;
   bool r;

   /* create the index so that "dropIndexes" can succeed */
   r = mongoc_database_write_command_with_opts (
      test->session_db,
      tmp_bson ("{'createIndexes': '%s',"
                " 'indexes': [{'key': {'a': 1}, 'name': 'foo'}]}",
                test->session_collection->collection),
      &test->opts,
      NULL,
      &error);

   ASSERT_OR_PRINT (r, error);

   test->succeeded = mongoc_collection_drop_index_with_opts (
      test->collection, "foo", &test->opts, &test->error);
}

static void
test_create_index (session_test_t *test)
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
test_replace_one (session_test_t *test)
{
   test->succeeded = mongoc_collection_replace_one (test->collection,
                                                    tmp_bson ("{}"),
                                                    tmp_bson ("{}"),
                                                    &test->opts,
                                                    NULL,
                                                    &test->error);
}

static void
test_update_one (session_test_t *test)
{
   test->succeeded =
      mongoc_collection_update_one (test->collection,
                                    tmp_bson ("{}"),
                                    tmp_bson ("{'$set': {'x': 1}}"),
                                    &test->opts,
                                    NULL,
                                    &test->error);
}

static void
test_update_many (session_test_t *test)
{
   test->succeeded =
      mongoc_collection_update_many (test->collection,
                                     tmp_bson ("{}"),
                                     tmp_bson ("{'$set': {'x': 1}}"),
                                     &test->opts,
                                     NULL,
                                     &test->error);
}

static void
test_insert_one (session_test_t *test)
{
   test->succeeded = mongoc_collection_insert_one (
      test->collection, tmp_bson ("{}"), &test->opts, NULL, &test->error);
}

static void
test_insert_many (session_test_t *test)
{
   bson_t *docs[2] = {tmp_bson ("{}"), tmp_bson ("{}")};
   test->succeeded = mongoc_collection_insert_many (test->collection,
                                                    (const bson_t **) docs,
                                                    2,
                                                    &test->opts,
                                                    NULL,
                                                    &test->error);
}

static void
test_delete_one (session_test_t *test)
{
   test->succeeded = mongoc_collection_delete_one (
      test->collection, tmp_bson ("{}"), &test->opts, NULL, &test->error);
}

static void
test_delete_many (session_test_t *test)
{
   test->succeeded = mongoc_collection_delete_many (
      test->collection, tmp_bson ("{}"), &test->opts, NULL, &test->error);
}

static void
test_rename (session_test_t *test)
{
   mongoc_collection_t *collection;

   /* ensure "rename" can succeed */
   insert_10_docs (test);

   /* mongoc_collection_rename_with_opts mutates the struct! */
   collection = mongoc_collection_copy (test->collection);
   test->succeeded = mongoc_collection_rename_with_opts (
      collection, "db", "newname", true, &test->opts, &test->error);

   mongoc_collection_destroy (collection);
}

static void
test_fam (session_test_t *test)
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
test_db_drop (session_test_t *test)
{
   test->succeeded =
      mongoc_database_drop_with_opts (test->db, &test->opts, &test->error);
}

static void
test_gridfs_find (session_test_t *test)
{
   mongoc_gridfs_t *gfs;
   bson_error_t error;
   mongoc_gridfs_file_list_t *list;
   mongoc_gridfs_file_t *f;

   /* work around lack of mongoc_client_get_gridfs_with_opts for now, can't yet
    * include lsid with the GridFS createIndexes command */
   mongoc_client_set_apm_callbacks (test->client, NULL, NULL);
   gfs = mongoc_client_get_gridfs (test->client, "test", NULL, &error);
   ASSERT_OR_PRINT (gfs, error);
   set_session_test_callbacks (test);
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
test_gridfs_find_one (session_test_t *test)
{
   mongoc_gridfs_t *gfs;
   bson_error_t error;
   mongoc_gridfs_file_t *f;

   /* work around lack of mongoc_client_get_gridfs_with_opts for now, can't yet
    * include lsid with the GridFS createIndexes command */
   mongoc_client_set_apm_callbacks (test->client, NULL, NULL);
   gfs = mongoc_client_get_gridfs (test->client, "test", NULL, &error);
   ASSERT_OR_PRINT (gfs, error);
   set_session_test_callbacks (test);
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
   bson_t opts;
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   /* ensure multiple batches */
   insert_10_docs (test);

   bson_copy_to (&test->opts, &opts);
   BSON_APPEND_INT32 (&opts, "batchSize", 2);

   cursor = mongoc_collection_aggregate (
      test->collection, MONGOC_QUERY_NONE, tmp_bson ("{}"), &opts, NULL);

   while (mongoc_cursor_next (cursor, &doc)) {
   }

   test->succeeded = !mongoc_cursor_error (cursor, &test->error);

   mongoc_cursor_destroy (cursor);
   bson_destroy (&opts);
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
test_find_databases (session_test_t *test)
{
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   cursor = mongoc_client_find_databases_with_opts (test->client, &test->opts);

   mongoc_cursor_next (cursor, &doc);
   test->succeeded = !mongoc_cursor_error (cursor, &test->error);
   mongoc_cursor_destroy (cursor);
}


static void
test_find_collections (session_test_t *test)
{
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   cursor = mongoc_database_find_collections_with_opts (test->db, &test->opts);

   mongoc_cursor_next (cursor, &doc);
   test->succeeded = !mongoc_cursor_error (cursor, &test->error);
   mongoc_cursor_destroy (cursor);
}


static void
test_collection_names (session_test_t *test)
{
   char **strv;

   strv = mongoc_database_get_collection_names_with_opts (
      test->db, &test->opts, &test->error);
   test->succeeded = (strv != NULL);
   bson_strfreev (strv);
}


static void
test_bulk (session_test_t *test)
{
   mongoc_bulk_operation_t *bulk;
   uint32_t i;

   bulk = mongoc_collection_create_bulk_operation_with_opts (test->collection,
                                                             &test->opts);

   test->succeeded = mongoc_bulk_operation_insert_with_opts (
      bulk, tmp_bson ("{}"), NULL, &test->error);
   check_success (test);

   test->succeeded = mongoc_bulk_operation_update_one_with_opts (
      bulk,
      tmp_bson ("{}"),
      tmp_bson ("{'$set': {'x': 1}}"),
      NULL,
      &test->error);
   check_success (test);

   test->succeeded = mongoc_bulk_operation_remove_one_with_opts (
      bulk, tmp_bson ("{}"), NULL, &test->error);
   check_success (test);

   i = mongoc_bulk_operation_execute (bulk, NULL, &test->error);
   test->succeeded = (i != 0);

   mongoc_bulk_operation_destroy (bulk);
}


static void
test_find_indexes (session_test_t *test)
{
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   /* ensure the collection exists so the listIndexes command succeeds */
   insert_10_docs (test);

   cursor =
      mongoc_collection_find_indexes_with_opts (test->collection, &test->opts);

   mongoc_cursor_next (cursor, &doc);
   test->succeeded = !mongoc_cursor_error (cursor, &test->error);
   mongoc_cursor_destroy (cursor);
}


#define add_session_test(_suite, _name, _test_fn)             \
   TestSuite_AddFull (_suite,                                 \
                      _name,                                  \
                      run_session_test,                       \
                      NULL,                                   \
                      (void *) (_test_fn),                    \
                      test_framework_skip_if_no_cluster_time, \
                      test_framework_skip_if_no_crypto)

#define add_session_test_wc(_suite, _name, _test_fn, ...)     \
   TestSuite_AddFull (_suite,                                 \
                      _name,                                  \
                      run_session_test,                       \
                      NULL,                                   \
                      (void *) (_test_fn),                    \
                      test_framework_skip_if_no_cluster_time, \
                      test_framework_skip_if_no_crypto,       \
                      __VA_ARGS__)


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
                      test_framework_skip_if_no_sessions,
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
   TestSuite_AddFull (suite,
                      "/Session/supported/single",
                      test_session_supported_single,
                      NULL,
                      NULL,
                      TestSuite_CheckLive,
                      test_framework_skip_if_no_crypto);
   TestSuite_AddFull (suite,
                      "/Session/supported/pooled",
                      test_session_supported_pooled,
                      NULL,
                      NULL,
                      TestSuite_CheckLive,
                      test_framework_skip_if_no_crypto);
   TestSuite_AddMockServerTest (suite,
                                "/Session/end/mock/single",
                                test_mock_end_sessions_single,
                                test_framework_skip_if_no_crypto);
   TestSuite_AddMockServerTest (suite,
                                "/Session/end/mock/pooled",
                                test_mock_end_sessions_pooled,
                                test_framework_skip_if_no_crypto);
   TestSuite_AddFull (suite,
                      "/Session/end/single",
                      test_end_sessions_single,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_crypto,
                      test_framework_skip_if_max_wire_version_less_than_6);
   TestSuite_AddFull (suite,
                      "/Session/end/pooled",
                      test_end_sessions_pooled,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_crypto,
                      test_framework_skip_if_max_wire_version_less_than_6);
   TestSuite_AddFull (suite,
                      "/Session/advance_cluster_time",
                      test_session_advance_cluster_time,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_crypto,
                      test_framework_skip_if_no_sessions);
   TestSuite_AddFull (suite,
                      "/Session/advance_operation_time",
                      test_session_advance_operation_time,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_crypto,
                      test_framework_skip_if_no_sessions);
   add_session_test (suite, "/Session/read_cmd", test_read_cmd);
   add_session_test (suite, "/Session/db_cmd", test_db_cmd);
   add_session_test (suite, "/Session/cmd", test_cmd);
   add_session_test (suite, "/Session/count", test_count);
   add_session_test (suite, "/Session/cursor", test_cursor);
   add_session_test (suite, "/Session/drop", test_drop);
   add_session_test (suite, "/Session/drop_index", test_drop_index);
   add_session_test (suite, "/Session/create_index", test_create_index);
   add_session_test (suite, "/Session/replace_one", test_replace_one);
   add_session_test (suite, "/Session/update_one", test_update_one);
   add_session_test (suite, "/Session/update_many", test_update_many);
   add_session_test (suite, "/Session/insert_one", test_insert_one);
   add_session_test (suite, "/Session/insert_many", test_insert_many);
   add_session_test (suite, "/Session/delete_one", test_delete_one);
   add_session_test (suite, "/Session/delete_many", test_delete_many);
   add_session_test (suite, "/Session/rename", test_rename);
   add_session_test (suite, "/Session/fam", test_fam);
   add_session_test (suite, "/Session/db_drop", test_db_drop);
   add_session_test (suite, "/Session/gridfs_find", test_gridfs_find);
   add_session_test (suite, "/Session/gridfs_find_one", test_gridfs_find_one);
   add_session_test_wc (suite,
                        "/Session/watch",
                        test_watch,
                        test_framework_skip_if_not_rs_version_6);
   add_session_test (suite, "/Session/aggregate", test_aggregate);
   add_session_test (suite, "/Session/create", test_create);
   add_session_test (suite, "/Session/database_names", test_database_names);
   add_session_test (suite, "/Session/find_databases", test_find_databases);
   add_session_test (suite, "/Session/find_collections", test_find_collections);
   add_session_test (suite, "/Session/collection_names", test_collection_names);
   add_session_test (suite, "/Session/bulk", test_bulk);
   add_session_test (suite, "/Session/find_indexes", test_find_indexes);
}
