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

#include "TestSuite.h"
#include "test-libmongoc.h"
#include "mongoc/mongoc.h"
#include "mock_server/mock-server.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "mock_server/request.h"
#include "test-conveniences.h"

// Example test of the first hello / isMaster sent on a mongoc_client_t.
bool
_test_mongoc_hello_impl (int requested_server_api_version)
{
   mock_server_t *server;
   mongoc_client_t *client;
   future_t *future;
   bson_error_t error;
   bson_t *ping;
   request_t *request;
   bool ret;

   mongoc_server_api_t *requested_server_api = NULL;

   /* The test_framework_client_pool_new_from_uri() function checks that the
    requested API can be set; if NULL is passed, it uses the default server API
    via a call to test_framework_get_default_server_api(), which here will
    basically come from an environment variable; otherwise, we'll use the
    version that has been requested: */
   if (-1 != requested_server_api_version) {
      requested_server_api =
         mongoc_server_api_new (requested_server_api_version);
      ASSERT (requested_server_api);
   }

   MONGOC_DEBUG (
      "using requested_server_api_version == %d; requested_server_api = %p\n",
      requested_server_api_version,
      (void *) requested_server_api);

   server = mock_server_new ();
   mock_server_run (server);

   client = test_framework_client_new_from_uri (mock_server_get_uri (server),
                                                requested_server_api);
   ASSERT (client);

   ping = BCON_NEW ("ping", BCON_INT32 (1));

   // Use a "future" function to send a ping command in the background. 
   future = future_client_command_simple (
      client, "db", ping, NULL, NULL, &error);

   /* Since this is the first command, a new connection is opened. */
   if(-1 != requested_server_api_version) {
   	request = mock_server_receives_hello(server);
   }
   else {
	// legacy API:
   	request = mock_server_receives_legacy_hello (server, "{'isMaster': 1}");
   } 
   ASSERT (request);

   // Note that this basically works against the mock server as there's no "real" legacy mode:
   mock_server_replies_simple (
      request, "{'ok': 1, 'isWritablePrimary': true, 'maxWireVersion': 14 }");
   request_destroy (request);

   // Now expect the ping command that we launched earlier:
   request = mock_server_receives_msg (
      server, MONGOC_MSG_NONE, tmp_bson ("{'ping': 1}"));
   ASSERT (request);
   mock_server_replies_simple (request,
                               "{'ok': 1, 'isWritablePrimary': true }");
   request_destroy (request);

   ret = future_get_bool (future);
   ASSERT (ret);

   // Tidy up:
   future_destroy (future);
   bson_destroy (ping);
   mongoc_client_destroy (client);
   mock_server_destroy (server);

   /* If we made it here, everything went just fine: */
   return true;
}

void
test_mongoc_hello ()
{
   if (!TestSuite_CheckMockServerAllowed ()) {
      return;
   }

   // Always check with the default protocol version (which here may come from the
   // environment; this is "legacy hello" in this test):
   _test_mongoc_hello_impl (-1);

   // Check with non-legacy hello:
   _test_mongoc_hello_impl (MONGOC_SERVER_API_V1);
}

bool
_test_mongoc_hello_client_pool_impl (int requested_server_api_version)
{
   mock_server_t *server;
   mongoc_uri_t *test_uri;
   mongoc_client_pool_t *pool;
   mongoc_client_t *client; 
   request_t *request;

   mongoc_server_api_t *requested_server_api = NULL;

   if (-1 != requested_server_api_version) {
      requested_server_api =
         mongoc_server_api_new (requested_server_api_version);
      ASSERT (requested_server_api);
   }

   MONGOC_DEBUG (
      "using requested_server_api_version == %d; requested_server_api = %p\n",
      requested_server_api_version,
      (void *) requested_server_api);

   server = mock_server_new ();
   mock_server_run (server);

   test_uri = mongoc_uri_copy (mock_server_get_uri (server));

   pool =
      test_framework_client_pool_new_from_uri (test_uri, requested_server_api);
   BSON_ASSERT (pool);

   test_framework_set_pool_ssl_opts (pool);

   client = mongoc_client_pool_pop (pool);
   ASSERT(client);

   // Popping the client should have triggered a connection:
   if(-1 != requested_server_api_version) {
        // a specific API version has been requested:
   	request = mock_server_receives_hello(server);
   }
   else {
	// legacy API:
   	request = mock_server_receives_legacy_hello (server, "{'isMaster': 1}");
   } 

   // Check the response from "hello":
   mock_server_replies_simple (
      request, "{'ok': 1, 'isWritablePrimary': true, 'maxWireVersion': 14 }");
   request_destroy (request);

   // Return the client to the pool: 
   mongoc_client_pool_push (pool, client);

   // Tidy up:
   mongoc_client_pool_destroy (pool);
   mongoc_uri_destroy (test_uri);
   mongoc_server_api_destroy (requested_server_api);

   /* If we're here, things went ok: */
   return true;
}

void
test_mongoc_hello_client_pool ()
{
   if (!TestSuite_CheckMockServerAllowed ()) {
      return;
   }

   _test_mongoc_hello_client_pool_impl (-1);
   _test_mongoc_hello_client_pool_impl (MONGOC_SERVER_API_V1);
}

void
test_hello_install (TestSuite *suite)
{
   TestSuite_AddMockServerTest (suite, "/hello", test_mongoc_hello),
      TestSuite_AddMockServerTest (
         suite, "/hello/client_pool", test_mongoc_hello_client_pool);
}
