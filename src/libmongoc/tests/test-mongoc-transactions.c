#include <mongoc.h>

#include "mongoc-collection-private.h"

#include "json-test.h"
#include "test-libmongoc.h"
#include "mock_server/mock-rs.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "json-test-operations.h"


static void
transactions_test_run_operation (json_test_ctx_t *ctx,
                                 const bson_t *test,
                                 const bson_t *operation)
{
   mongoc_client_session_t *session = NULL;

   if (bson_has_field (operation, "arguments.session")) {
      session = session_from_name (
         ctx, bson_lookup_utf8 (operation, "arguments.session"));
   }

   /* expect some warnings from abortTransaction, but don't suppress others: we
    * want to know if any other tests log warnings */
   capture_logs (true);
   json_test_operation (ctx, test, operation, session);
   assert_all_captured_logs_have_prefix ("Error in abortTransaction:");
   capture_logs (false);
}


static void
test_transactions_cb (bson_t *scenario)
{
   json_test_config_t config = JSON_TEST_CONFIG_INIT;
   config.run_operation_cb = transactions_test_run_operation;
   config.scenario = scenario;
   config.command_started_events_only = true;
   run_json_general_test (&config);
}


static void
test_transactions_supported (void *ctx)
{
   bool supported;
   mongoc_client_t *client;
   mongoc_client_session_t *session;
   mongoc_database_t *db;
   mongoc_collection_t *collection;
   bson_t opts = BSON_INITIALIZER;
   bson_error_t error;
   bool r;

   supported = test_framework_max_wire_version_at_least (7) &&
               test_framework_is_replset ();
   client = test_framework_client_new ();
   mongoc_client_set_error_api (client, 2);
   db = mongoc_client_get_database (client, "transaction-tests");

   /* drop and create collection outside of transaction */
   collection = mongoc_database_create_collection (db, "test", NULL, &error);
   if (!collection && error.domain == MONGOC_ERROR_SERVER && error.code == 48) {
      /* already exists */
      collection = mongoc_database_get_collection (db, "test");
   } else {
      ASSERT_OR_PRINT (collection, error);
   }

   session = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (session, error);

   /* Transactions Spec says "startTransaction SHOULD report an error if the
    * driver can detect that transactions are not supported by the deployment",
    * but we take advantage of the wiggle room and don't error here. */
   r = mongoc_client_session_start_transaction (session, NULL, &error);
   ASSERT_OR_PRINT (r, error);

   r = mongoc_client_session_append (session, &opts, &error);
   ASSERT_OR_PRINT (r, error);
   r = mongoc_collection_insert_one (
      collection, tmp_bson ("{}"), &opts, NULL, &error);

   if (supported) {
      ASSERT_OR_PRINT (r, error);
   } else {
      BSON_ASSERT (!r);
      ASSERT_CMPINT32 (error.domain, ==, MONGOC_ERROR_SERVER);
      ASSERT_CONTAINS (error.message, "transaction");
   }

   bson_destroy (&opts);
   mongoc_collection_destroy (collection);

   if (!supported) {
      /* suppress "error in abortTransaction" warning from session_destroy */
      capture_logs (true);
   }

   mongoc_client_session_destroy (session);
   mongoc_database_destroy (db);
   mongoc_client_destroy (client);
}


void
test_transactions_install (TestSuite *suite)
{
   char resolved[PATH_MAX];

   ASSERT (realpath (JSON_DIR "/transactions", resolved));
   install_json_test_suite_with_check (
      suite,
      resolved,
      test_transactions_cb,
      test_framework_skip_if_no_crypto,
      test_framework_skip_if_no_sessions,
      test_framework_skip_if_not_replset,
      test_framework_skip_if_max_wire_version_less_than_7);

   TestSuite_AddFull (suite,
                      "/transactions/supported",
                      test_transactions_supported,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_sessions,
                      test_framework_skip_if_no_crypto);
}
