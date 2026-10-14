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

#include <mongoc/mongoc-client-private.h>
#include <mongoc/mongoc-cyrus-private.h>

#include <mongoc/mongoc.h>

#include <TestSuite.h>
#include <test-conveniences.h>
#include <test-libmongoc.h>

#include <limits.h>


static void
test_sasl_properties(void)
{
   mongoc_uri_t *uri;
   mongoc_cyrus_t sasl;

   uri = mongoc_uri_new("mongodb://user@host/?authMechanism=GSSAPI&"
                        "authMechanismProperties=SERVICE_NAME:sn,CANONICALIZE_HOST_NAME:TrUe");

   BSON_ASSERT(uri);
   memset(&sasl, 0, sizeof sasl);
   _mongoc_sasl_set_properties((mongoc_sasl_t *)&sasl, uri);

   ASSERT(sasl.credentials.canonicalize_host_name);
   ASSERT_CMPSTR(sasl.credentials.service_name, "sn");

   mongoc_uri_destroy(uri);

   capture_logs(true);
   /* authMechanismProperties take precedence */
   uri = mongoc_uri_new("mongodb://user@host/?authMechanism=GSSAPI&"
                        "canonicalizeHostname=true&gssapiServiceName=blah&"
                        "authMechanismProperties=SERVICE_NAME:sn,CANONICALIZE_HOST_NAME:False");

   ASSERT_CAPTURED_LOG("authMechanismProperties should overwrite gssapiServiceName",
                       MONGOC_LOG_LEVEL_WARNING,
                       "Overwriting previously provided value for 'authmechanismproperties'");

   _mongoc_cyrus_destroy(&sasl);
   memset(&sasl, 0, sizeof sasl);
   _mongoc_sasl_set_properties((mongoc_sasl_t *)&sasl, uri);

   ASSERT(!sasl.credentials.canonicalize_host_name);
   ASSERT_CMPSTR(sasl.credentials.service_name, "sn");

   _mongoc_cyrus_destroy(&sasl);
   mongoc_uri_destroy(uri);
}


#define CANON_OUT_MAX 255u

static void
test_sasl_canon_user(void)
{
   char *const out = bson_malloc(CANON_OUT_MAX);
   unsigned out_len = 0u;
   const char in[] = {'u', 's', 'e', 'r'};

   ASSERT_CMPINT(
      _mongoc_cyrus_canon_user(NULL, NULL, in, sizeof in, 0u, NULL, out, CANON_OUT_MAX, &out_len), ==, SASL_OK);

   // The output is deliberately not NUL terminated; Cyrus-SASL reads exactly `out_len` bytes.
   ASSERT_CMPUINT(out_len, ==, (unsigned)sizeof in);
   ASSERT(0 == memcmp(out, in, sizeof in));

   bson_free(out);
}

// A username of exactly `out_max` bytes is a full-length username, not an overflow.
static void
test_sasl_canon_user_longest_accepted(void)
{
   const unsigned inlen = CANON_OUT_MAX;
   char *const out = bson_malloc(inlen);
   char *const in = bson_malloc(inlen);
   unsigned out_len = 0u;

   memset(in, 'x', inlen);

   ASSERT_CMPINT(_mongoc_cyrus_canon_user(NULL, NULL, in, inlen, 0u, NULL, out, CANON_OUT_MAX, &out_len), ==, SASL_OK);

   ASSERT_CMPUINT(out_len, ==, inlen);
   ASSERT(0 == memcmp(out, in, inlen));

   bson_free(in);
   bson_free(out);
}

// Regression test for CDRIVER-6416.
static void
test_sasl_canon_user_too_large(void)
{
   const unsigned oversized[] = {CANON_OUT_MAX + 1u, UINT_MAX - 1u, UINT_MAX};

   capture_logs(true);

   for (size_t i = 0u; i < sizeof oversized / sizeof *oversized; i++) {
      char *const out = bson_malloc(CANON_OUT_MAX);
      unsigned out_len = 0u;
      const char in[] = {'x'};

      ASSERT_WITH_MSG(_mongoc_cyrus_canon_user(NULL, NULL, in, oversized[i], 0u, NULL, out, CANON_OUT_MAX, &out_len) ==
                         SASL_BUFOVER,
                      "expected SASL_BUFOVER for inlen %u",
                      oversized[i]);

      ASSERT_CAPTURED_LOG("oversized SASL username", MONGOC_LOG_LEVEL_ERROR, "SASL username too large");
      clear_captured_logs();

      bson_free(out);
   }

   capture_logs(false);
}

#undef CANON_OUT_MAX

static void
test_sasl_canonicalize_hostname(void *ctx)
{
   mongoc_client_t *client;
   mongoc_server_stream_t *ss;
   char real_name[BSON_HOST_NAME_MAX + 1] = {'\0'};
   bson_error_t error;

   BSON_UNUSED(ctx);

   client = test_framework_new_default_client();
   ss = mongoc_cluster_stream_for_reads(&client->cluster, TEST_SS_LOG_CONTEXT, NULL, NULL, NULL, NULL, &error);
   ASSERT_OR_PRINT(ss, error);

   BSON_ASSERT(_mongoc_sasl_get_canonicalized_name(ss->stream, real_name, sizeof real_name));

   ASSERT_CMPSIZE_T(strlen(real_name), >, (size_t)0);

   mongoc_server_stream_cleanup(ss);
   mongoc_client_destroy(client);
}


void
test_cyrus_install(TestSuite *suite)
{
   TestSuite_Add(suite, "/SASL/properties", test_sasl_properties);
   TestSuite_Add(suite, "/SASL/canon_user", test_sasl_canon_user);
   TestSuite_Add(suite, "/SASL/canon_user/longest_accepted", test_sasl_canon_user_longest_accepted);
   TestSuite_Add(suite, "/SASL/canon_user/too_large", test_sasl_canon_user_too_large);
   TestSuite_AddFull(suite,
                     "/SASL/canonicalize [lock:live-server]",
                     test_sasl_canonicalize_hostname,
                     NULL,
                     NULL,
                     TestSuite_CheckLive,
                     test_framework_skip_if_offline);
}
