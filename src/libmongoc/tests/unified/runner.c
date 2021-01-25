/*
 * Copyright 2020-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bsonutil/bson-parser.h"
#include "entity-map.h"
#include "json-test.h"
#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include <signal.h>

/* TODO: put in separate files? runner.c, file.c, test.c, diagnostics.c */

/* test_runner_t, test_file_t, and test_t model the types described in the "Test
 * Runner Implementation" section of the Unified Test Format specification. */
typedef struct {
   mongoc_client_t *internal_client;
   semver_t server_version;
   /* topology_type may be "single", "replicaset", "sharded", or
    * "sharded-replicaset". */
   const char *topology_type;
   mongoc_array_t server_ids;
} test_runner_t;

typedef struct {
   test_runner_t *test_runner;

   char *description;
   semver_t schema_version;
   bson_t *run_on_requirements;
   bson_t *create_entities;
   bson_t *initial_data;
   bson_t *tests;
} test_file_t;

typedef struct {
   test_file_t *test_file;

   char *description;
   bson_t *run_on_requirements;
   char *skip_reason;
   bson_t *operations;
   bson_t *expect_events;
   bson_t *outcome;
   entity_map_t *entity_map;
} test_t;

/* test_diagnostics is a global storing current test state.
 * When an assertion fails, causing an abort signal, the test
 * diagnostics are logged. */
struct {
   test_runner_t *test_runner;
   test_file_t *test_file;
   test_t *test;
} test_diagnostics;

static const char *
get_topology_type (mongoc_client_t *client);

static bool
is_topology_type_sharded (const char *topology_type)
{
   return 0 == strcmp ("sharded", topology_type) ||
          0 == strcmp ("sharded-replicaset", topology_type);
}

static bool
is_topology_type_compatible (const char *test_topology_type,
                             const char *server_topology_type)
{
   if (0 == strcmp (test_topology_type, server_topology_type)) {
      return true;
   }
   /* If a requirement specifies a "sharded" topology and server is of type
    * "sharded-replicaset", that is also compatible. */
   return 0 == strcmp (test_topology_type, "sharded") &&
          is_topology_type_sharded (server_topology_type);
}

/* This callback tracks the set of server IDs for all connected servers.
 * The set of server IDs is used when sending a command to each individual
 * server.
 */
static void
on_topology_changed (const mongoc_apm_topology_changed_t *event)
{
   test_runner_t *test_runner;
   const mongoc_topology_description_t *td;
   mongoc_server_description_t **sds;
   size_t sds_len;
   size_t i;

   test_runner =
      (test_runner_t *) mongoc_apm_topology_changed_get_context (event);
   _mongoc_array_clear (&test_runner->server_ids);
   td = mongoc_apm_topology_changed_get_new_description (event);
   sds = mongoc_topology_description_get_servers (td, &sds_len);
   for (i = 0; i < sds_len; i++) {
      uint32_t server_id = mongoc_server_description_id (sds[i]);
      MONGOC_DEBUG ("Topology changed, adding server id: %d", (int) server_id);
      _mongoc_array_append_val (&test_runner->server_ids, server_id);
   }
   mongoc_server_descriptions_destroy_all (sds, sds_len);
}

/* Returns an array of all known servers IDs that the test runner
 * is connected to. The server IDs can be used to target commands to
 * specific servers with mongoc_client_command_simple_with_server_id().
 */
static void
test_runner_get_all_server_ids (test_runner_t *test_runner, mongoc_array_t *out)
{
   bson_error_t error;
   bool ret;

   /* Run a 'ping' command to make sure topology has been scanned. */
   ret = mongoc_client_command_simple (test_runner->internal_client,
                                       "admin",
                                       tmp_bson ("{'ping': 1}"),
                                       NULL /* read prefs */,
                                       NULL /* reply */,
                                       NULL /* error */);
   ASSERT_OR_PRINT (ret, error);

   _mongoc_array_copy (out, &test_runner->server_ids);
}

/* Run killAllSessions against the primary or each mongos to terminate any
 * lingering open transactions.
 * See also: Spec section "Terminating Open Transactions"
 */
static void
test_runner_terminate_open_transactions (test_runner_t *test_runner)
{
   bson_t *kill_all_sessions_cmd;
   bool ret;
   bson_error_t error;

   if (0 == test_framework_skip_if_no_txns ()) {
      MONGOC_DEBUG ("Sessions not supported, not running killAllSessions");
      return;
   }

   kill_all_sessions_cmd = tmp_bson ("{'killAllSessions': []}");
   /* Run on each mongos. Target each server individually. */
   if (is_topology_type_sharded (test_runner->topology_type)) {
      mongoc_array_t server_ids;
      size_t i;

      _mongoc_array_init (&server_ids, sizeof (uint32_t));
      test_runner_get_all_server_ids (test_runner, &server_ids);
      for (i = 0; i < server_ids.len; i++) {
         uint32_t server_id = _mongoc_array_index (&server_ids, uint32_t, i);

         ret = mongoc_client_command_simple_with_server_id (
            test_runner->internal_client,
            "admin",
            kill_all_sessions_cmd,
            NULL /* read prefs. */,
            server_id,
            NULL,
            &error);

         /* Ignore error code 11601 as a workaround for SERVER-38335. */
         if (!ret && error.code != 11601) {
            test_error (
               "Unexpected error running killAllSessions on server (%d): %s",
               (int) server_id,
               error.message);
         }
      }
      _mongoc_array_destroy (&server_ids);
      return;
   }

   /* Run on primary. */
   ret = mongoc_client_command_simple (test_runner->internal_client,
                                       "admin",
                                       kill_all_sessions_cmd,
                                       NULL /* read prefs. */,
                                       NULL,
                                       &error);

   /* Ignore error code 11601 as a workaround for SERVER-38335. */
   if (!ret && error.code != 11601) {
      test_error ("Unexpected error running killAllSessions on primary: %s",
                  error.message);
   }
}

static test_runner_t *
test_runner_new (void)
{
   test_runner_t *test_runner;
   mongoc_apm_callbacks_t *callbacks;

   test_runner = bson_malloc0 (sizeof (test_runner_t));
   /* Create an client for internal test operations (e.g. checking server
    * version) */
   _mongoc_array_init (&test_runner->server_ids, sizeof (uint32_t));
   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_topology_changed_cb (callbacks, on_topology_changed);
   test_runner->internal_client = test_framework_client_new ();
   mongoc_client_set_apm_callbacks (
      test_runner->internal_client, callbacks, test_runner);
   mongoc_client_set_error_api (test_runner->internal_client,
                                MONGOC_ERROR_API_VERSION_2);
   test_runner->topology_type =
      get_topology_type (test_runner->internal_client);
   server_semver (test_runner->internal_client, &test_runner->server_version);
   test_diagnostics.test_runner = test_runner;

   /* Terminate any possible open transactions. */
   test_runner_terminate_open_transactions (test_runner);
   mongoc_apm_callbacks_destroy (callbacks);
   return test_runner;
}

static void
test_runner_destroy (test_runner_t *test_runner)
{
   test_diagnostics.test_runner = NULL;
   mongoc_client_destroy (test_runner->internal_client);
   _mongoc_array_destroy (&test_runner->server_ids);
   bson_free (test_runner);
}

static test_file_t *
test_file_new (test_runner_t *test_runner, bson_t *bson)
{
   test_file_t *test_file;
   bson_parser_t *parser;
   char *schema_version;

   test_file = bson_malloc0 (sizeof (test_file_t));
   test_file->test_runner = test_runner;

   parser = bson_parser_new ();
   bson_parser_utf8 (parser, "description", &test_file->description);
   bson_parser_utf8 (parser, "schemaVersion", &schema_version);
   bson_parser_array_optional (
      parser, "runOnRequirements", &test_file->run_on_requirements);
   bson_parser_array_optional (
      parser, "createEntities", &test_file->create_entities);
   bson_parser_array_optional (parser, "initialData", &test_file->initial_data);
   bson_parser_array (parser, "tests", &test_file->tests);
   bson_parser_parse_or_assert (parser, bson);
   bson_parser_destroy (parser);

   semver_parse (schema_version, &test_file->schema_version);
   bson_free (schema_version);
   test_diagnostics.test_file = test_file;
   return test_file;
}

static void
test_file_destroy (test_file_t *test_file)
{
   test_diagnostics.test_file = NULL;
   bson_free (test_file->description);
   bson_destroy (test_file->tests);
   bson_destroy (test_file->initial_data);
   bson_destroy (test_file->create_entities);
   bson_destroy (test_file->run_on_requirements);
   bson_free (test_file);
}

static test_t *
test_new (test_file_t *test_file, bson_t *bson)
{
   test_t *test;
   bson_parser_t *parser;

   test = bson_malloc0 (sizeof (test_t));
   test->test_file = test_file;
   parser = bson_parser_new ();
   bson_parser_utf8 (parser, "description", &test->description);
   bson_parser_array_optional (
      parser, "runOnRequirements", &test->run_on_requirements);
   bson_parser_utf8_optional (parser, "skipReason", &test->skip_reason);
   bson_parser_array (parser, "operations", &test->operations);
   bson_parser_array_optional (parser, "expectEvents", &test->expect_events);
   bson_parser_array_optional (parser, "outcome", &test->outcome);
   bson_parser_parse_or_assert (parser, bson);
   bson_parser_destroy (parser);

   test->entity_map = entity_map_new ();
   test_diagnostics.test = test;
   return test;
}

static void
test_destroy (test_t *test)
{
   test_diagnostics.test = NULL;
   entity_map_destroy (test->entity_map);
   bson_destroy (test->outcome);
   bson_destroy (test->expect_events);
   bson_destroy (test->operations);
   bson_destroy (test->run_on_requirements);
   bson_free (test->description);
   bson_free (test->skip_reason);
   bson_free (test);
}

static bool
is_replset (bson_t *ismaster_reply)
{
   if (bson_has_field (ismaster_reply, "setName")) {
      return true;
   }

   if (bson_has_field (ismaster_reply, "isreplicaset") &&
       bson_lookup_bool (ismaster_reply, "isreplicaset") == true) {
      return true;
   }

   return false;
}

static bool
is_sharded (bson_t *ismaster_reply)
{
   const char *val;
   if (!bson_has_field (ismaster_reply, "msg")) {
      return false;
   }


   val = bson_lookup_utf8 (ismaster_reply, "msg");
   if (0 == strcmp (val, "isdbgrid")) {
      return true;
   }
   return false;
}

static const char *
get_topology_type (mongoc_client_t *client)
{
   bool ret;
   bson_t reply;
   bson_error_t error;
   const char *topology_type = "single";

   ret = mongoc_client_command_simple (
      client, "admin", tmp_bson ("{'ismaster': 1}"), NULL, &reply, &error);
   ASSERT_OR_PRINT (ret, error);

   if (is_replset (&reply)) {
      topology_type = "replicaset";
   } else if (is_sharded (&reply)) {
      bool is_sharded_replset;
      mongoc_collection_t *config_shards;
      mongoc_cursor_t *cursor;
      const bson_t *shard_doc;

      /* Check if this is a sharded-replicaset by querying the config.shards
       * collection. */
      is_sharded_replset = true;
      config_shards = mongoc_client_get_collection (client, "config", "shards");
      cursor = mongoc_collection_find_with_opts (config_shards,
                                                 tmp_bson ("{}"),
                                                 NULL /* opts */,
                                                 NULL /* read prefs */);
      if (mongoc_cursor_error (cursor, &error)) {
         test_error ("Attempting to query config.shards collection failed: %s",
                     error.message);
      }
      while (mongoc_cursor_next (cursor, &shard_doc)) {
         const char *host = bson_lookup_utf8 (shard_doc, "host");
         if (NULL == strstr (host, "/")) {
            is_sharded_replset = false;
            break;
         }
      }

      mongoc_cursor_destroy (cursor);
      mongoc_collection_destroy (config_shards);

      if (is_sharded_replset) {
         topology_type = "sharded-replicaset";
      } else {
         topology_type = "sharded";
      }
   }

   bson_destroy (&reply);
   return topology_type;
}

static void
handle_abort (int signo)
{
   MONGOC_ERROR ("Test aborting");

   if (test_diagnostics.test_file) {
      MONGOC_ERROR ("Test file description: %s",
                    test_diagnostics.test_file->description);
   }

   if (test_diagnostics.test) {
      MONGOC_ERROR ("Test description: %s", test_diagnostics.test->description);
   }
}

static void
test_diagnostics_init (void)
{
   memset (&test_diagnostics, 0, sizeof (test_diagnostics));
   signal (SIGABRT, handle_abort);
}

static void
test_diagnostics_cleanup (void)
{
   signal (SIGABRT, SIG_DFL);
}

static void
check_schema_version (test_file_t *test_file)
{
   const char *supported_version_strs[] = {"1.0"};
   int i;

   for (i = 0; i < sizeof (supported_version_strs) /
                      sizeof (supported_version_strs[0]);
        i++) {
      semver_t supported_version;

      semver_parse (supported_version_strs[i], &supported_version);
      if (supported_version.major != test_file->schema_version.major) {
         continue;
      }
      if (!supported_version.has_minor) {
         /* All minor versions for this major version are supported. */
         return;
      }
      if (supported_version.minor >= test_file->schema_version.minor) {
         return;
      }
   }

   test_error ("Unsupported schema version: %s",
               semver_to_string (&test_file->schema_version));
}

static bool
check_run_on_requirement (bson_t *run_on_requirement,
                          const char *server_topology_type,
                          semver_t *server_version,
                          char **fail_reason)
{
   bson_iter_t req_iter;

   BSON_FOREACH (run_on_requirement, req_iter)
   {
      const char *key = bson_iter_key (&req_iter);

      if (0 == strcmp (key, "minServerVersion")) {
         semver_t min_server_version;

         semver_parse (bson_iter_utf8 (&req_iter, NULL), &min_server_version);
         if (semver_cmp (server_version, &min_server_version) < 0) {
            *fail_reason = bson_strdup_printf (
               "Server version(%s) is lower than minServerVersion(%s)",
               semver_to_string (server_version),
               semver_to_string (&min_server_version));
            return false;
         }
         continue;
      }

      if (0 == strcmp (key, "maxServerVersion")) {
         semver_t max_server_version;

         semver_parse (bson_iter_utf8 (&req_iter, NULL), &max_server_version);
         if (semver_cmp (server_version, &max_server_version) > 0) {
            *fail_reason = bson_strdup_printf (
               "Server version(%s) is higher than maxServerVersion (%s)",
               semver_to_string (server_version),
               semver_to_string (&max_server_version));
            return false;
         }
         continue;
      }

      if (0 == strcmp (key, "topologies")) {
         bool found = false;
         bson_t topologies;
         bson_iter_t topology_iter;

         bson_iter_bson (&req_iter, &topologies);
         BSON_FOREACH (&topologies, topology_iter)
         {
            const char *test_topology_type =
               bson_iter_utf8 (&topology_iter, NULL);
            if (is_topology_type_compatible (test_topology_type,
                                             server_topology_type)) {
               found = true;
               break;
            }
         }

         if (!found) {
            *fail_reason = bson_strdup_printf (
               "Topology (%s) was not found among listed topologies: %s",
               server_topology_type,
               tmp_json (&topologies));
            return false;
         }
         continue;
      }

      test_error ("Unexpected runOnRequirement field: %s", key);
   }
   return true;
}

static bool
check_run_on_requirements (test_runner_t *test_runner,
                           bson_t *run_on_requirements,
                           const char **reason)
{
   bson_string_t *fail_reasons;
   bool requirements_satisfied = false;
   bson_iter_t iter;

   fail_reasons = bson_string_new ("");
   BSON_FOREACH (run_on_requirements, iter)
   {
      bson_t run_on_requirement;
      char *fail_reason;

      bson_iter_bson (&iter, &run_on_requirement);
      fail_reason = NULL;
      if (check_run_on_requirement (&run_on_requirement,
                                    test_runner->topology_type,
                                    &test_runner->server_version,
                                    &fail_reason)) {
         requirements_satisfied = true;
         break;
      }

      bson_string_append_printf (fail_reasons,
                                 "- Requirement %s failed because: %s\n",
                                 bson_iter_key (&iter),
                                 fail_reason);
      bson_free (fail_reason);
   }

   *reason = NULL;
   if (!requirements_satisfied) {
      (*reason) =
         tmp_str ("runOnRequirements not satified:\n%s", fail_reasons->str);
   }
   bson_string_free (fail_reasons, true);
   return requirements_satisfied;
}

static bool
test_setup_initial_data (test_t *test, bson_error_t *error)
{
   test_runner_t *test_runner;
   test_file_t *test_file;
   bson_iter_t initial_data_iter;

   test_file = test->test_file;
   test_runner = test_file->test_runner;

   if (!test_file->initial_data) {
      return true;
   }

   BSON_FOREACH (test_file->initial_data, initial_data_iter)
   {
      bson_parser_t *parser = NULL;
      bson_t collection_data;
      char *collection_name = NULL;
      char *database_name = NULL;
      bson_t *documents = NULL;
      mongoc_database_t *db = NULL;
      mongoc_collection_t *coll = NULL;
      mongoc_bulk_operation_t *bulk_insert = NULL;
      mongoc_write_concern_t *wc = NULL;
      bson_t *wc_opts = NULL;
      bson_t *drop_opts = NULL;
      bool ret = false;

      bson_iter_bson (&initial_data_iter, &collection_data);
      parser = bson_parser_new ();
      bson_parser_utf8 (parser, "databaseName", &database_name);
      bson_parser_utf8 (parser, "collectionName", &collection_name);
      bson_parser_array (parser, "documents", &documents);
      if (!bson_parser_parse (parser, &collection_data, error)) {
         goto loopexit;
      }

      wc = mongoc_write_concern_new ();
      mongoc_write_concern_set_w (wc, MONGOC_WRITE_CONCERN_W_MAJORITY);
      wc_opts = bson_new ();
      mongoc_write_concern_append (wc, wc_opts);

      /* Drop the collection. */
      /* Check if the server supports majority write concern on 'drop'. */
      if (semver_cmp_str (&test_runner->server_version, "3.4") >= 0) {
         drop_opts = bson_new ();
         mongoc_write_concern_append (wc, drop_opts);
      }
      coll = mongoc_client_get_collection (
         test_runner->internal_client, database_name, collection_name);
      if (!mongoc_collection_drop_with_opts (coll, drop_opts, error)) {
         if (error->code != 26 &&
             (NULL == strstr (error->message, "ns not found"))) {
            /* This is not a "ns not found" error. Fail the test. */
            goto loopexit;
         }
      }

      /* Insert documents if specified. */
      if (bson_count_keys (documents) > 0) {
         bson_iter_t documents_iter;

         bulk_insert =
            mongoc_collection_create_bulk_operation_with_opts (coll, wc_opts);

         BSON_FOREACH (documents, documents_iter)
         {
            bson_t document;

            bson_iter_bson (&documents_iter, &document);
            mongoc_bulk_operation_insert (bulk_insert, &document);
         }

         if (!mongoc_bulk_operation_execute (bulk_insert, NULL, error)) {
            goto loopexit;
         }
      } else {
         /* Test does not need data inserted, just create the collection. */
         db = mongoc_client_get_database (test_runner->internal_client,
                                          database_name);
         if (!mongoc_database_create_collection (
                db, collection_name, wc_opts, error)) {
            goto loopexit;
         }
      }

      ret = true;

   loopexit:
      mongoc_bulk_operation_destroy (bulk_insert);
      bson_destroy (wc_opts);
      bson_destroy (drop_opts);
      bson_destroy (documents);
      mongoc_write_concern_destroy (wc);
      mongoc_collection_destroy (coll);
      bson_free (database_name);
      bson_free (collection_name);
      bson_parser_destroy (parser);
      if (!ret) {
         return false;
      }
   }
   return true;
}

static bool
test_create_entities (test_t *test, bson_error_t *error)
{
   test_file_t *test_file;
   bson_iter_t iter;

   test_file = test->test_file;

   if (!test_file->create_entities) {
      return true;
   }

   BSON_FOREACH (test_file->create_entities, iter)
   {
      bson_t entity_bson;

      bson_iter_bson (&iter, &entity_bson);
      if (!entity_map_create (test->entity_map, &entity_bson, error)) {
         return false;
      }
   }
   return true;
}

/* This returns an error on failure instead of asserting where possible.
 * This allows the test runner to perform server clean up even on failure (e.g.
 * disable failpoints).
 */
bool
test_run (test_t *test, bson_error_t *error)
{
   test_runner_t *test_runner;
   test_file_t *test_file;

   test_file = test->test_file;
   test_runner = test_file->test_runner;

   if (test->skip_reason != NULL) {
      MONGOC_DEBUG ("SKIPPING test '%s'. Reason: '%s'",
                    test->description,
                    test->skip_reason);
      return true;
   }

   if (test->run_on_requirements) {
      const char *reason;
      if (!check_run_on_requirements (
             test_runner, test->run_on_requirements, &reason)) {
         MONGOC_DEBUG ("SKIPPING test '%s'. Reason: '%s'",
                       test->description,
                       test->skip_reason);
         return true;
      }
   }

   if (!test_setup_initial_data (test, error)) {
      return false;
   }

   if (!test_create_entities (test, error)) {
      return false;
   }

   return true;
}

void
run_one_test_file (bson_t *bson)
{
   test_runner_t *test_runner;
   test_file_t *test_file;
   bson_iter_t test_iter;

   test_diagnostics_init ();

   test_runner = test_runner_new ();
   test_file = test_file_new (test_runner, bson);

   MONGOC_DEBUG ("running test file: %s", test_file->description);

   check_schema_version (test_file);
   if (test_file->run_on_requirements) {
      const char *reason;
      if (!check_run_on_requirements (
             test_runner, test_file->run_on_requirements, &reason)) {
         MONGOC_DEBUG ("SKIPPING test file (%s). Reason:\n%s",
                       test_file->description,
                       reason);
         goto done;
      }
   }

   BSON_FOREACH (test_file->tests, test_iter)
   {
      test_t *test;
      bson_t test_bson;
      bool test_ok;
      bson_error_t error;

      bson_iter_bson (&test_iter, &test_bson);
      test = test_new (test_file, &test_bson);
      test_ok = test_run (test, &error);
      if (!test_ok) {
         test_error ("Test '%s' failed: %s", test->description, error.message);
      }
      test_destroy (test);
   }

done:
   test_file_destroy (test_file);
   test_runner_destroy (test_runner);
   test_diagnostics_cleanup ();
}

void
test_install_unified (TestSuite *suite)
{
   char resolved[PATH_MAX];

   ASSERT (realpath (JSON_DIR "/unified", resolved));

   install_json_test_suite_with_check (suite,
                                       resolved,
                                       &run_one_test_file,
                                       TestSuite_CheckLive,
                                       test_framework_skip_if_no_crypto);
}
