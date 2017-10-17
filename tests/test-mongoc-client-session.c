#include "mongoc.h"
#include "mongoc-util-private.h"
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
                          MONGOC_ERROR_CLIENT_AUTHENTICATE,
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
}
