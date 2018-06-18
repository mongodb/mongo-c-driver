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
   bson_t *majority = tmp_bson ("{'writeConcern': {'w': 'majority'}}");
   bson_t opts = BSON_INITIALIZER;
   bson_error_t error;
   bool r;

   if (test_framework_is_mongos ()) {
      return;
   }

   supported = test_framework_max_wire_version_at_least (7) &&
               test_framework_is_replset ();
   client = test_framework_client_new ();
   mongoc_client_set_error_api (client, 2);
   db = mongoc_client_get_database (client, "transaction-tests");

   /* drop and create collection outside of transaction */
   mongoc_database_write_command_with_opts (
      db, tmp_bson ("{'drop': 'test'}"), majority, NULL, NULL);
   collection =
      mongoc_database_create_collection (db, "test", majority, &error);
   ASSERT_OR_PRINT (collection, error);

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


static void
test_server_selection_error (void)
{
   mock_rs_t *rs;
   mongoc_client_t *client;
   mongoc_client_session_t *session;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   mongoc_bulk_operation_t *bulk;
   mongoc_find_and_modify_opts_t *fam;
   bson_t opts = BSON_INITIALIZER;
   bson_error_t error;
   bson_t *b;
   bson_t *u;
   const bson_t *doc_out;
   const bson_t *error_doc;
   bson_t reply;
   bool r;

   rs = mock_rs_with_autoismaster (7 /* wire version */,
                                   true /* has primary */,
                                   0 /* secondaries */,
                                   0 /* arbiters */);

   mock_rs_run (rs);
   client = mongoc_client_new_from_uri (mock_rs_get_uri (rs));
   session = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (session, error);
   r = mongoc_client_session_start_transaction (session, NULL, &error);
   ASSERT_OR_PRINT (r, error);
   r = mongoc_client_session_append (session, &opts, &error);
   ASSERT_OR_PRINT (r, error);
   collection = mongoc_client_get_collection (client, "db", "collection");

   /* stop responding */
   mock_rs_destroy (rs);
   /* warnings when trying to abort the transaction and later, end sessions */
   capture_logs (true);

#define ASSERT_TRANSIENT_LABEL(_b, _expr)                                \
   do {                                                                  \
      if (!mongoc_error_has_label ((_b), "TransientTransactionError")) { \
         test_error ("Reply lacks TransientTransactionError label: %s\n" \
                     "Running %s",                                       \
                     bson_as_json ((_b), NULL),                          \
                     #_expr);                                            \
      }                                                                  \
   } while (0)

#define TEST_SS_ERR(_expr)                    \
   do {                                       \
      r = (_expr);                            \
      BSON_ASSERT (!r);                       \
      ASSERT_TRANSIENT_LABEL (&reply, _expr); \
      bson_destroy (&reply);                  \
      /* clean slate for next test */         \
      memset (&reply, 0, sizeof (reply));     \
   } while (0)

#define TEST_SS_ERR_CURSOR(_cursor_expr)                              \
   do {                                                               \
      cursor = (_cursor_expr);                                        \
      r = mongoc_cursor_next (cursor, &doc_out);                      \
      BSON_ASSERT (!r);                                               \
      r = !mongoc_cursor_error_document (cursor, &error, &error_doc); \
      BSON_ASSERT (!r);                                               \
      BSON_ASSERT (error_doc);                                        \
      ASSERT_TRANSIENT_LABEL (error_doc, _cursor_expr);               \
      mongoc_cursor_destroy (cursor);                                 \
   } while (0)

   b = tmp_bson ("{'x': 1}");
   u = tmp_bson ("{'$inc': {'x': 1}}");

   TEST_SS_ERR (mongoc_client_command_with_opts (
      client, "db", b, NULL, &opts, &reply, NULL));
   TEST_SS_ERR (mongoc_client_read_command_with_opts (
      client, "db", b, NULL, &opts, &reply, NULL));
   TEST_SS_ERR (mongoc_client_write_command_with_opts (
      client, "db", b, &opts, &reply, NULL));
   TEST_SS_ERR (mongoc_client_read_write_command_with_opts (
      client, "db", b, NULL, &opts, &reply, NULL));
   TEST_SS_ERR (
      mongoc_collection_insert_one (collection, b, &opts, &reply, NULL));
   TEST_SS_ERR (mongoc_collection_insert_many (
      collection, (const bson_t **) &b, 1, &opts, &reply, NULL));
   TEST_SS_ERR (
      mongoc_collection_update_one (collection, b, u, &opts, &reply, NULL));
   TEST_SS_ERR (
      mongoc_collection_update_many (collection, b, u, &opts, &reply, NULL));
   TEST_SS_ERR (
      mongoc_collection_replace_one (collection, b, b, &opts, &reply, NULL));
   TEST_SS_ERR (
      mongoc_collection_delete_one (collection, b, &opts, &reply, NULL));
   TEST_SS_ERR (
      mongoc_collection_delete_many (collection, b, &opts, &reply, NULL));
   TEST_SS_ERR (0 < mongoc_collection_count_documents (
                       collection, b, &opts, NULL, &reply, NULL));

   TEST_SS_ERR_CURSOR (mongoc_collection_aggregate (
      collection, MONGOC_QUERY_NONE, tmp_bson ("[{}]"), &opts, NULL));
   TEST_SS_ERR_CURSOR (
      mongoc_collection_find_with_opts (collection, b, &opts, NULL));

   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, &opts);
   mongoc_bulk_operation_insert (bulk, b);
   TEST_SS_ERR (mongoc_bulk_operation_execute (bulk, &reply, NULL));

   fam = mongoc_find_and_modify_opts_new ();
   mongoc_find_and_modify_opts_append (fam, &opts);
   TEST_SS_ERR (mongoc_collection_find_and_modify_with_opts (
      collection, b, fam, &reply, NULL));

   BEGIN_IGNORE_DEPRECATIONS;
   TEST_SS_ERR (mongoc_collection_create_index_with_opts (
      collection, b, NULL, &opts, &reply, NULL));
   END_IGNORE_DEPRECATIONS

   mongoc_find_and_modify_opts_destroy (fam);
   mongoc_bulk_operation_destroy (bulk);
   bson_destroy (&opts);
   mongoc_collection_destroy (collection);
   mongoc_client_session_destroy (session);
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
      test_transactions_cb,test_framework_skip_if_no_txns);

   /* skip mongos for now - txn support coming in 4.1.0 */
   TestSuite_AddFull (suite,
                      "/transactions/supported",
                      test_transactions_supported,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_sessions,
                      test_framework_skip_if_no_crypto,
                      test_framework_skip_if_mongos);
   TestSuite_AddMockServerTest (suite,
                                "/transactions/server_selection_err",
                                test_server_selection_error,
                                test_framework_skip_if_no_crypto);
}
