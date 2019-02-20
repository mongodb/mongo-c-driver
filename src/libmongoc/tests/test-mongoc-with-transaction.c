#include <mongoc/mongoc.h>

#include "json-test.h"
#include "mongoc/mongoc-client-session-private.h"
#include "test-libmongoc.h"

typedef struct _cb_ctx_t {
   bson_t callback;
   json_test_ctx_t *ctx;
} cb_ctx_t;


static bool
with_transaction_callback_runner (mongoc_client_session_t *session,
                                  void *ctx,
                                  bson_t **reply,
                                  bson_error_t *error)
{
   cb_ctx_t *cb_ctx = (cb_ctx_t *) ctx;
   bson_t local_reply;
   bson_t operation;
   bson_t operations;
   bson_t *test;
   bson_iter_t iter;
   bool res = false;

   test = &(cb_ctx->callback);

   if (bson_has_field (test, "operation")) {
      bson_lookup_doc (test, "operation", &operation);
      res = json_test_operation (cb_ctx->ctx,
                                 test,
                                 &operation,
                                 cb_ctx->ctx->collection,
                                 session,
                                 &local_reply);
   } else {
      ASSERT (bson_has_field (test, "operations"));
      bson_lookup_doc (test, "operations", &operations);
      BSON_ASSERT (bson_iter_init (&iter, &operations));

      bson_init (&local_reply);

      while (bson_iter_next (&iter)) {
         bson_destroy (&local_reply);
         bson_iter_bson (&iter, &operation);
         res = json_test_operation (cb_ctx->ctx,
                                    test,
                                    &operation,
                                    cb_ctx->ctx->collection,
                                    session,
                                    &local_reply);
         if (!res) {
            break;
         }
      }
   }

   *reply = bson_copy (&local_reply);
   bson_destroy (&local_reply);

   return res;
}

static bool
with_transaction_test_run_operation (json_test_ctx_t *ctx,
                                     const bson_t *test,
                                     const bson_t *operation)
{
   mongoc_transaction_opt_t *opts = NULL;
   mongoc_client_session_t *session = NULL;
   bson_error_t error;
   bson_t args;
   bson_t reply;
   bool res;
   cb_ctx_t cb_ctx;

   bson_lookup_doc (operation, "arguments", &args);

   /* If there is a 'callback' field, run the nested operations through
      mongoc_client_session_with_transaction(). */
   if (bson_has_field (&args, "callback")) {
      bson_init (&reply);

      ASSERT (bson_has_field (operation, "object"));
      session = session_from_name (ctx, bson_lookup_utf8 (operation, "object"));
      ASSERT (session);

      bson_lookup_doc (&args, "callback", &cb_ctx.callback);
      cb_ctx.ctx = ctx;

      if (bson_has_field (&args, "options")) {
         opts = bson_lookup_txn_opts (&args, "options");
      }

      res = mongoc_client_session_with_transaction (
         session, with_transaction_callback_runner, opts, &cb_ctx, &error);

   } else {
      /* If there is no 'callback' field, then run simply. */
      if (bson_has_field (&args, "session")) {
         session = session_from_name (ctx, bson_lookup_utf8 (&args, "session"));
      }

      res = json_test_operation (
         ctx, test, operation, ctx->collection, session, &reply);
   }

   bson_destroy (&args);
   bson_destroy (&reply);
   mongoc_transaction_opts_destroy (opts);

   return res;
}

static void
test_with_transaction_cb (bson_t *scenario)
{
   json_test_config_t config = JSON_TEST_CONFIG_INIT;

   config.run_operation_cb = with_transaction_test_run_operation;
   config.scenario = scenario;
   config.command_started_events_only = true;

   run_json_general_test (&config);
}

static void
test_all_spec_tests (TestSuite *suite)
{
   char resolved[PATH_MAX];

   test_framework_resolve_path (JSON_DIR "/with_transaction", resolved);

   install_json_test_suite_with_check (suite,
                                       resolved,
                                       test_with_transaction_cb,
                                       test_framework_skip_if_no_txns);
}

static bool
with_transaction_fail_transient_txn (mongoc_client_session_t *session,
                                     void *ctx,
                                     bson_t **reply,
                                     bson_error_t *error)
{
   bson_t labels;

   _mongoc_usleep (session->with_txn_timeout_ms * 1000);

   *reply = bson_new ();
   BSON_APPEND_ARRAY_BEGIN (*reply, "errorLabels", &labels);
   BSON_APPEND_UTF8 (&labels, "0", TRANSIENT_TXN_ERR);

   return false;
}

static bool
with_transaction_do_nothing (mongoc_client_session_t *session,
                             void *ctx,
                             bson_t **reply,
                             bson_error_t *error)
{
   return true;
}

static void
test_with_transaction_timeout (void *ctx)
{
   mongoc_client_t *client;
   mongoc_client_session_t *session;
   bson_error_t error;
   bool res;

   client = test_framework_client_new ();

   session = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (session, error);

   session->with_txn_timeout_ms = 10;

   /* Test Case 1: Test that if the callback returns an
      error with the TransientTransactionError label and
      we have exceeded the timeout, withTransaction fails. */
   res = mongoc_client_session_with_transaction (
      session, with_transaction_fail_transient_txn, NULL, NULL, &error);
   ASSERT (!res);

   /* Test Case 2: If committing returns an error with the
      UnknownTransactionCommitResult label and we have exceeded
      the timeout, withTransaction fails. */
   session->fail_commit_label = UNKNOWN_COMMIT_RESULT;
   res = mongoc_client_session_with_transaction (
      session, with_transaction_do_nothing, NULL, NULL, &error);
   ASSERT (!res);

   /* Test Case 3: If committing returns an error with the
      TransientTransactionError label and we have exceeded the
      timeout, withTransaction fails. */
   session->fail_commit_label = TRANSIENT_TXN_ERR;
   res = mongoc_client_session_with_transaction (
      session, with_transaction_do_nothing, NULL, NULL, &error);
   ASSERT (!res);

   mongoc_client_session_destroy (session);
   mongoc_client_destroy (client);
}

void
test_with_transaction_install (TestSuite *suite)
{
   test_all_spec_tests (suite);

   TestSuite_AddFull (suite,
                      "/with_transaction/timeout_tests",
                      test_with_transaction_timeout,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_sessions,
                      test_framework_skip_if_no_crypto);
}
