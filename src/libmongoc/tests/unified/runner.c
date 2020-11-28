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

#include "json-test.h"
#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include <signal.h>

/* The test context is a global storing state for each test.
 * When an assertion fails, causing an abort signal, the test
 * context can be logged. */
struct {
   bson_t *test;
   mongoc_uri_t *uri;
   const char *description;
   bson_t command_started_events;
   bson_t command_succeeded_events;
   bson_t command_failed_events;
   mongoc_client_t *internal_client;
   /* topology_type may be: {single, replicaset, sharded, sharded-replicaset} */
   const char *topology_type;
} test_ctx;

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
test_ctx_init (bson_t *test)
{
   bson_error_t error;
   char *uri_str;

   test_ctx.test = test;
   test_ctx.description = bson_lookup_utf8 (test, "description");
   bson_init (&test_ctx.command_started_events);
   bson_init (&test_ctx.command_succeeded_events);
   bson_init (&test_ctx.command_failed_events);

   /* Create an client for internal test operations (e.g. checking server
    * version) */
   if (test_framework_getenv ("MONGOC_TEST_URI") != NULL) {
      uri_str = test_framework_getenv ("MONGOC_TEST_URI");
   } else {
      uri_str = bson_strdup ("mongodb://localhost:27017");
   }

   test_ctx.uri = mongoc_uri_new_with_error (uri_str, &error);
   ASSERT_OR_PRINT (test_ctx.uri, error);
   test_ctx.internal_client = mongoc_client_new_from_uri (test_ctx.uri);
   bson_free (uri_str);
}

static void
test_ctx_cleanup ()
{
   mongoc_uri_destroy (test_ctx.uri);
   mongoc_client_destroy (test_ctx.internal_client);
   bson_destroy (&test_ctx.command_started_events);
   bson_destroy (&test_ctx.command_succeeded_events);
   bson_destroy (&test_ctx.command_failed_events);
}

static void
handle_abort (int signo)
{
   printf ("Test aborting: '%s'\n", test_ctx.description);
   printf ("URI: %s\n", mongoc_uri_get_string (test_ctx.uri));
   printf ("Topology type: %s\n", test_ctx.topology_type);
   printf ("command started events: %s\n",
           tmp_json (&test_ctx.command_started_events));
   printf ("command succeeded events: %s\n",
           tmp_json (&test_ctx.command_succeeded_events));
   printf ("command succeeded events: %s\n",
           tmp_json (&test_ctx.command_failed_events));
}

static void
check_schema_version (void)
{
   const char *supported_version_strs[] = {"1.0"};
   const char *test_version_str;
   semver_t test_version;
   int i;

   test_version_str = bson_lookup_utf8 (test_ctx.test, "schemaVersion");
   semver_parse (test_version_str, &test_version);

   for (i = 0; i < sizeof (supported_version_strs) /
                      sizeof (supported_version_strs[0]);
        i++) {
      semver_t supported_version;

      semver_parse (supported_version_strs[i], &supported_version);
      if (supported_version.major != test_version.major) {
         continue;
      }
      if (!supported_version.has_minor) {
         /* All minor versions for this major version are supported. */
         return;
      }
      if (supported_version.minor >= test_version.minor) {
         return;
      }
   }

   test_error ("Unsupported schema version: %s", test_version_str);
}

static bool
check_run_on_requirement (bson_t *run_on_requirement,
                          const char *server_topology_type,
                          semver_t *server_version,
                          char **fail_reason)
{
   BSON_FOREACH_BEGIN (run_on_requirement, reqiter)
   {
      const char *key = bson_iter_key (&reqiter);

      if (0 == strcmp (key, "minServerVersion")) {
         semver_t min_server_version;

         semver_parse (bson_iter_utf8 (&reqiter, NULL), &min_server_version);
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

         semver_parse (bson_iter_utf8 (&reqiter, NULL), &max_server_version);
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

         bson_iter_bson (&reqiter, &topologies);
         BSON_FOREACH_BEGIN (&topologies, topology_iter)
         {
            const char *topology = bson_iter_utf8 (&topology_iter, NULL);
            if (0 == strcmp (topology, server_topology_type)) {
               found = true;
               break;
            } else if (0 == strcmp (topology, "sharded") &&
                       0 ==
                          strcmp (server_topology_type, "sharded-replicaset")) {
               /* If a requirement specifies a "sharded" topology and we are
                * connected to a "sharded-replicaset", that is ok. */
               found = true;
               break;
            }
         }
         BSON_FOREACH_END;

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
   BSON_FOREACH_END;
   return true;
}

static void
check_run_on_requirements (void)
{
   bson_t run_on_requirements;
   bson_string_t *fail_reasons;
   semver_t server_version;
   bool requirements_satisfied = false;

   if (!bson_has_field (test_ctx.test, "runOnRequirements")) {
      return;
   }

   /* Get the topology and version of the connected deployment. */
   server_semver (test_ctx.internal_client, &server_version);
   test_ctx.topology_type = get_topology_type (test_ctx.internal_client);

   bson_lookup_doc (test_ctx.test, "runOnRequirements", &run_on_requirements);

   fail_reasons = bson_string_new ("");
   BSON_FOREACH_BEGIN (&run_on_requirements, iter)
   {
      bson_t run_on_requirement;
      char *fail_reason;

      bson_iter_bson (&iter, &run_on_requirement);
      fail_reason = NULL;
      if (check_run_on_requirement (&run_on_requirement,
                                    test_ctx.topology_type,
                                    &server_version,
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
   BSON_FOREACH_END;

   if (!requirements_satisfied) {
      test_error ("No runOnRequirements were satisfied:\n%s",
                  fail_reasons->str);
   }
   bson_string_free (fail_reasons, true);
}

void
run_one_test (bson_t *test)
{
   test_ctx_init (test);
   MONGOC_DEBUG ("running test: %s", test_ctx.description);
   signal (SIGABRT, handle_abort);
   check_schema_version ();
   check_run_on_requirements ();
   signal (SIGABRT, SIG_DFL);
   test_ctx_cleanup ();
}

void
test_install_unified (TestSuite *suite)
{
   char resolved[PATH_MAX];

   ASSERT (realpath (JSON_DIR "/unified", resolved));

   install_json_test_suite_with_check (suite,
                                       resolved,
                                       &run_one_test,
                                       TestSuite_CheckLive,
                                       test_framework_skip_if_no_crypto);
}
