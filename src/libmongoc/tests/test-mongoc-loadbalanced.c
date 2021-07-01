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

#include "mongoc/mongoc.h"
#include "mongoc/mongoc-client-session-private.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include "TestSuite.h"

static char *loadbalanced_uri (void) {
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
   mongoc_client_session_t *session;
   char *uristr = loadbalanced_uri ();
   bson_error_t error;
   bson_t *lsid1;
   bson_t *lsid2;

   client = mongoc_client_new (uristr);
   session = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session, error);

   lsid1 = bson_copy (&session->server_session->lsid);
   session->server_session->last_used_usec = 1;
   mongoc_client_session_destroy (session);

   session = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session, error);
   lsid2 = bson_copy (&session->server_session->lsid);
   mongoc_client_session_destroy (session);

   if (!bson_equal (lsid1, lsid2)) {
      test_error ("Session not reused: %s != %s", tmp_json (lsid1), tmp_json (lsid2));
   }

   bson_destroy (lsid1);
   bson_destroy (lsid2);
   bson_free (uristr);
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
   TestSuite_AddFull (suite, "/loadbalanced/sessions/supported",
                      test_loadbalanced_sessions_supported,
                      NULL /* ctx */,
                      NULL /* dtor */,
                      skip_if_not_loadbalanced);
   TestSuite_AddFull (suite, "/loadbalanced/sessions/do_not_expire",
                      test_loadbalanced_sessions_do_not_expire,
                      NULL /* ctx */,
                      NULL /* dtor */,
                      skip_if_not_loadbalanced);
}
