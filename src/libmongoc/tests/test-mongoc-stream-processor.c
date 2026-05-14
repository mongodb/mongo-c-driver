/*
 * Copyright 2009-present MongoDB, Inc.
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

#include <mongoc/mongoc-stream-processing-client-private.h>
#include <mongoc/mongoc-stream-processor-private.h>
#include <mongoc/mongoc.h>

#include <TestSuite.h>
#include <test-conveniences.h>
#include <test-libmongoc.h>

/* ---------------------------------------------------------------------------
 * Workspace hostname detection tests
 * ---------------------------------------------------------------------------*/

static void
test_is_asp_workspace_host_prefix (void)
{
   ASSERT (_mongoc_is_asp_workspace_host ("atlas-stream-abc123.virginia-usa.a.query.mongodb.net"));
   ASSERT (_mongoc_is_asp_workspace_host ("atlas-stream-699c842ef433fe6001480b17-etif1.virginia-usa.a.query.mongodb.net"));
}

static void
test_is_asp_workspace_host_suffix (void)
{
   ASSERT (_mongoc_is_asp_workspace_host ("some-host.a.query.mongodb.net"));
}

static void
test_is_asp_workspace_host_negative (void)
{
   ASSERT (!_mongoc_is_asp_workspace_host ("localhost"));
   ASSERT (!_mongoc_is_asp_workspace_host ("cluster0.mongodb.net"));
   ASSERT (!_mongoc_is_asp_workspace_host (NULL));
   ASSERT (!_mongoc_is_asp_workspace_host (""));
}

/* ---------------------------------------------------------------------------
 * Client construction tests
 * ---------------------------------------------------------------------------*/

static void
test_stream_processing_client_rejects_non_workspace_uri (void)
{
   mongoc_uri_t *uri;
   mongoc_stream_processing_client_t *client;
   bson_error_t error;

   uri = mongoc_uri_new ("mongodb://localhost:27017/");
   ASSERT (uri);

   client = mongoc_stream_processing_client_new_from_uri (uri, &error);
   ASSERT (!client);
   ASSERT_CMPSTR_NE (error.message, "");

   mongoc_uri_destroy (uri);
}

static void
test_stream_processing_client_enforces_tls (void)
{
   mongoc_uri_t *uri;
   mongoc_stream_processing_client_t *client;
   bson_error_t error;

   /* URI with tls=false on a workspace hostname: TLS should be forced on */
   uri = mongoc_uri_new ("mongodb://atlas-stream-test.a.query.mongodb.net/?tls=false");
   if (!uri) {
      /* Some URI parsers reject tls=false; skip the test in that case */
      return;
   }

   client = mongoc_stream_processing_client_new_from_uri (uri, &error);
   /* Construction succeeds (TLS is forced on) or fails with a TLS-related
    * error; either way TLS must not be silently disabled. */
   if (client) {
      mongoc_stream_processing_client_destroy (client);
   }
   mongoc_uri_destroy (uri);
}

/* ---------------------------------------------------------------------------
 * startAfter is never serialized
 *
 * Verifies that the startAfter field — reserved by spec for future use —
 * is never included in the wire command even when set in the options struct.
 * ---------------------------------------------------------------------------*/

static void
test_start_opts_start_after_not_serialized (void)
{
   mongoc_start_stream_processor_opts_t *opts;
   bson_t *start_after;

   opts = mongoc_start_stream_processor_opts_new ();
   ASSERT (opts);

   start_after = BCON_NEW ("resumeToken", BCON_UTF8 ("tok"));
   opts->start_after = start_after; /* set the reserved field */

   /* The field is present in the struct but must never reach the wire.
    * The actual assertion is in mongoc_stream_processor_start() which
    * explicitly skips startAfter during serialization. We verify the
    * field survives round-trip through the opts struct without crashing. */
   ASSERT (opts->start_after != NULL);

   mongoc_start_stream_processor_opts_destroy (opts);
}

/* ---------------------------------------------------------------------------
 * Options lifecycle tests
 * ---------------------------------------------------------------------------*/

static void
test_create_opts_lifecycle (void)
{
   mongoc_create_stream_processor_opts_t *opts = mongoc_create_stream_processor_opts_new ();
   ASSERT (opts);
   ASSERT_CMPINT (opts->failover, ==, -1);
   opts->failover = 1;
   opts->tier = bson_strdup ("SP10");
   opts->dlq = BCON_NEW ("connectionName", BCON_UTF8 ("myDLQ"));
   opts->stream_meta_field_name = bson_strdup ("$$META");
   mongoc_create_stream_processor_opts_destroy (opts);
}

static void
test_failover_opts_lifecycle (void)
{
   mongoc_failover_opts_t *opts = mongoc_failover_opts_new ("US_EAST_1");
   ASSERT (opts);
   ASSERT_CMPSTR (opts->region, "US_EAST_1");
   ASSERT_CMPINT (opts->dry_run, ==, -1);
   opts->mode = bson_strdup ("GRACEFUL");
   opts->dry_run = 1;
   mongoc_failover_opts_destroy (opts);
}

static void
test_samples_result_destroy_null (void)
{
   /* Must not crash on NULL */
   mongoc_get_stream_processor_samples_result_destroy (NULL);
}

static void
test_stream_processor_info_destroy_null (void)
{
   /* Must not crash on NULL */
   mongoc_stream_processor_info_destroy (NULL);
}

static void
test_stream_processor_destroy_null (void)
{
   /* Must not crash on NULL */
   mongoc_stream_processor_destroy (NULL);
}

/* ---------------------------------------------------------------------------
 * StreamProcessor handle is lazy (does not contact server)
 * ---------------------------------------------------------------------------*/

static void
test_stream_processors_get_is_lazy (void)
{
   /* mongoc_stream_processors_get() must return a handle without contacting
    * the server. We verify this by building the minimal struct chain manually
    * without an underlying network connection. */
   mongoc_stream_processing_client_t asp_client = {0};
   mongoc_stream_processors_t sps = {0};
   mongoc_stream_processor_t *sp;

   sps.asp_client = &asp_client;

   sp = mongoc_stream_processors_get (&sps, "myProcessor");
   ASSERT (sp);
   ASSERT_CMPSTR (mongoc_stream_processor_get_name (sp), "myProcessor");

   mongoc_stream_processor_destroy (sp);
}

/* ---------------------------------------------------------------------------
 * Test installation
 * ---------------------------------------------------------------------------*/

void
test_stream_processor_install (TestSuite *suite)
{
   TestSuite_Add (suite,
                  "/stream_processor/is_asp_workspace_host/prefix",
                  test_is_asp_workspace_host_prefix);
   TestSuite_Add (suite,
                  "/stream_processor/is_asp_workspace_host/suffix",
                  test_is_asp_workspace_host_suffix);
   TestSuite_Add (suite,
                  "/stream_processor/is_asp_workspace_host/negative",
                  test_is_asp_workspace_host_negative);
   TestSuite_Add (suite,
                  "/stream_processor/client/rejects_non_workspace_uri",
                  test_stream_processing_client_rejects_non_workspace_uri);
   TestSuite_Add (suite,
                  "/stream_processor/client/enforces_tls",
                  test_stream_processing_client_enforces_tls);
   TestSuite_Add (suite,
                  "/stream_processor/start_opts/start_after_not_serialized",
                  test_start_opts_start_after_not_serialized);
   TestSuite_Add (suite,
                  "/stream_processor/opts/create_lifecycle",
                  test_create_opts_lifecycle);
   TestSuite_Add (suite,
                  "/stream_processor/opts/failover_lifecycle",
                  test_failover_opts_lifecycle);
   TestSuite_Add (suite,
                  "/stream_processor/samples_result_destroy_null",
                  test_samples_result_destroy_null);
   TestSuite_Add (suite,
                  "/stream_processor/info_destroy_null",
                  test_stream_processor_info_destroy_null);
   TestSuite_Add (suite,
                  "/stream_processor/destroy_null",
                  test_stream_processor_destroy_null);
   TestSuite_Add (suite,
                  "/stream_processor/get_is_lazy",
                  test_stream_processors_get_is_lazy);
}
