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
                                 const bson_t *operation,
                                 mongoc_collection_t *collection)
{
   const char *description;
   const char *session_name;
   mongoc_client_session_t *session;

   if (bson_has_field (operation, "arguments.session")) {
      session_name = bson_lookup_utf8 (operation, "arguments.session");
      if (!strcmp (session_name, "session0")) {
         session = ctx->sessions[0];
      } else if (!strcmp (session_name, "session1")) {
         session = ctx->sessions[1];
      } else {
         MONGOC_ERROR ("Unrecognized session name: %s", session_name);
         abort ();
      }
   } else {
      session = NULL;
   }

   description = bson_lookup_utf8 (test, "description");

   /* we log warnings from abortTransaction. suppress warnings that we expect,
    * but don't suppress all: we want to know if any other tests log warnings */
   if (!strcmp (description, "write conflict abort") ||
       !strcmp (description, "abort ignores TransactionAborted") ||
       !strcmp (description, "abort does not apply writeConcern")) {
      capture_logs (true);
   }

   /* json_test_operations() chose session0 or session1 from the
    * arguments.session field in the JSON test */
   json_test_operation (test, operation, collection, session);

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
}
