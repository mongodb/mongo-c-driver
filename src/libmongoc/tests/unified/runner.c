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
   const char *description;
   bson_t command_started_events;
   bson_t command_succeeded_events;
   bson_t command_failed_events;
} test_ctx;

static void
test_ctx_init (bson_t *test)
{
   test_ctx.test = test;
   test_ctx.description = bson_lookup_utf8 (test, "description");
   bson_init (&test_ctx.command_started_events);
   bson_init (&test_ctx.command_succeeded_events);
   bson_init (&test_ctx.command_failed_events);
}

static void
test_ctx_cleanup ()
{
   bson_destroy (&test_ctx.command_started_events);
   bson_destroy (&test_ctx.command_succeeded_events);
   bson_destroy (&test_ctx.command_failed_events);
}

static void
handle_abort (int signo)
{
   printf ("Test aborting: '%s'\n", test_ctx.description);
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

void
run_one_test (bson_t *test)
{
   test_ctx_init (test);
   MONGOC_DEBUG ("running test: %s", test_ctx.description);
   signal (SIGABRT, handle_abort);
   check_schema_version ();
   ASSERT (bson_has_field (test, "runOnRequirements"));
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
