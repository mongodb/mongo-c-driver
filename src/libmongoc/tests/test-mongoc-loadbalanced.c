/*
 * Copyright 2021-present MongoDB, Inc.
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

#include "mongoc/mongoc.h"
#include "mongoc/mongoc-client-session-private.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include "TestSuite.h"

static char *
loadbalanced_uri (void)
{
   /* TODO (CDRIVER-4062): This will need to add TLS and auth to the URI when
    * run in evergreen. */
   return test_framework_getenv ("SINGLE_MONGOS_LB_URI");
}

static void
test_loadbalanced_sessions_supported (void *unused)
{
   mongoc_client_t *client;
   mongoc_client_session_t *session;
   char *uristr = loadbalanced_uri ();
   bson_error_t error;

   client = mongoc_client_new (uristr);
   session = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session, error);

   mongoc_client_session_destroy (session);
   bson_free (uristr);
   mongoc_client_destroy (client);
}

static void
test_loadbalanced_sessions_do_not_expire (void *unused)
{
   mongoc_client_t *client;
   mongoc_client_session_t *session1;
   mongoc_client_session_t *session2;
   char *uristr = loadbalanced_uri ();
   bson_error_t error;
   bson_t *session1_lsid;
   bson_t *session2_lsid;

   client = mongoc_client_new (uristr);
   /* Start two sessions, to ensure that pooled sessions remain in the pool when
    * the pool is accessed. */
   session1 = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session1, error);
   session2 = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session2, error);

   session1_lsid = bson_copy (mongoc_client_session_get_lsid (session1));
   session2_lsid = bson_copy (mongoc_client_session_get_lsid (session2));

   /* Expire both sessions. */
   session1->server_session->last_used_usec = 1;
   session2->server_session->last_used_usec = 1;
   mongoc_client_session_destroy (session1);
   mongoc_client_session_destroy (session2);

   /* Get a new session, it should reuse the most recently pushed session2. */
   session2 = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session2, error);
   if (!bson_equal (mongoc_client_session_get_lsid (session2), session2_lsid)) {
      test_error ("Session not reused: %s != %s",
                  tmp_json (mongoc_client_session_get_lsid (session2)),
                  tmp_json (session2_lsid));
   }

   session1 = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session1, error);
   if (!bson_equal (mongoc_client_session_get_lsid (session1), session1_lsid)) {
      test_error ("Session not reused: %s != %s",
                  tmp_json (mongoc_client_session_get_lsid (session1)),
                  tmp_json (session1_lsid));
   }

   bson_destroy (session1_lsid);
   bson_destroy (session2_lsid);
   bson_free (uristr);
   mongoc_client_session_destroy (session1);
   mongoc_client_session_destroy (session2);
   mongoc_client_destroy (client);
}

/* Test that invalid loadBalanced URI configurations are validated during client
 * construction. */
static void
test_loadbalanced_client_uri_validation (void *unused)
{
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   bson_error_t error;
   bool ret;

   uri = mongoc_uri_new ("mongodb://localhost:27017");
   mongoc_uri_set_option_as_bool (uri, MONGOC_URI_LOADBALANCED, true);
   mongoc_uri_set_option_as_bool (uri, MONGOC_URI_DIRECTCONNECTION, true);
   client = mongoc_client_new_from_uri (uri);

   ret = mongoc_client_command_simple (client,
                                       "admin",
                                       tmp_bson ("{'ping': 1}"),
                                       NULL /* read prefs */,
                                       NULL /* reply */,
                                       &error);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_SERVER_SELECTION,
                          MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                          "URI with \"loadBalanced\" enabled must not contain "
                          "option \"directConnection\" enabled");
   BSON_ASSERT (!ret);

   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
}

static int
skip_if_not_loadbalanced (void)
{
   char *val = loadbalanced_uri ();
   if (!val) {
      return 0;
   }
   bson_free (val);
   return 1;
}

void
test_loadbalanced_install (TestSuite *suite)
{
   TestSuite_AddFull (suite,
                      "/loadbalanced/sessions/supported",
                      test_loadbalanced_sessions_supported,
                      NULL /* ctx */,
                      NULL /* dtor */,
                      skip_if_not_loadbalanced);
   TestSuite_AddFull (suite,
                      "/loadbalanced/sessions/do_not_expire",
                      test_loadbalanced_sessions_do_not_expire,
                      NULL /* ctx */,
                      NULL /* dtor */,
                      skip_if_not_loadbalanced);
   TestSuite_AddFull (suite,
                      "/loadbalanced/client_uri_validation",
                      test_loadbalanced_client_uri_validation,
                      NULL /* ctx */,
                      NULL /* dtor */,
                      NULL);
}
