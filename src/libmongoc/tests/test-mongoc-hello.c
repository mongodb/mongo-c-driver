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

/* In order for these tests to pass, in addition to a successful non-terminating
test, all of the following should /also/ be true: */
bool
_test_mongoc_should_check_hello ()
{
   return test_framework_max_wire_version_at_least (WIRE_VERSION_OP_MSG);
}

/* Example test of the first hello / isMaster sent on a mongoc_client_t.
 * TODO: test with a mongoc_client_pool_t, which uses a separate code-path. */
bool
_test_mongoc_hello_impl (int requested_server_api_version)
{
   mock_server_t *server;
   mongoc_client_t *client;
   bson_t *ping;
   bson_error_t error;
   future_t *future;
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
   }

   server = mock_server_new ();
   mock_server_run (server);

   client = test_framework_client_new_from_uri (mock_server_get_uri (server),
                                                requested_server_api);
   ASSERT (client);

   ping = BCON_NEW ("ping", BCON_INT32 (1));

   /* Use a "future" function to send a ping command in the background. */
   future = future_client_command_simple (
      client, "db", ping, NULL /* read_prefs */, NULL /* reply */, &error);

   /* Since this is the first command, a new connection is opened. */
   request = mock_server_receives_legacy_hello (server, "{'isMaster': 1}");
   ASSERT (request);
   mock_server_replies_simple (
      request, "{'ok': 1, 'isWritablePrimary': true, 'maxWireVersion': 14 }");
   request_destroy (request);

   /* Now expect the ping command. */
   request = mock_server_receives_msg (
      server, MONGOC_MSG_NONE, tmp_bson ("{'ping': 1}"));
   ASSERT (request);
   mock_server_replies_simple (request,
                               "{'ok': 1, 'isWritablePrimary': true }");
   request_destroy (request);

   ret = future_get_bool (future);
   ASSERT (ret);
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
   /* Be sure the test didn't pass when it should /not/ have: */

   // Check with the default protocol version (which here may come from the
   // environment):
   ASSERT (_test_mongoc_should_check_hello () && _test_mongoc_hello_impl (-1));

   // Check with a specific protocol version:
   ASSERT (_test_mongoc_should_check_hello () &&
           _test_mongoc_hello_impl (MONGOC_SERVER_API_V1));
}

bool
_test_mongoc_hello_client_pool_impl (int requested_server_api_version)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t
      *cl3; /* the command client, which we'll push back into the pool */
   bson_t *cmd;
   bson_error_t err;
   bool cmd_result;

   mongoc_server_api_t *requested_server_api = NULL;

   mongoc_uri_t *test_uri = test_framework_get_uri ();

   if (-1 != requested_server_api_version) {
      requested_server_api =
         mongoc_server_api_new (requested_server_api_version);
   }

   pool =
      test_framework_client_pool_new_from_uri (test_uri, requested_server_api);

   BSON_ASSERT (pool);
   test_framework_set_pool_ssl_opts (pool);

   /* Now, set up a couple of clients, and a ping: */
   (void) mongoc_client_pool_pop (pool);
   (void) mongoc_client_pool_pop (pool);

   cmd = BCON_NEW ("ping", BCON_INT32 (1));

   cl3 = mongoc_client_pool_pop (pool);

   cmd_result =
      mongoc_client_command_simple (cl3, "admin", cmd, NULL, NULL, &err);

   ASSERT_OR_PRINT (cmd_result, err);

   mongoc_client_pool_push (pool, cl3);

   // Tidy up:
   mongoc_client_pool_destroy (pool);
   mongoc_uri_destroy (test_uri);
   mongoc_server_api_destroy (requested_server_api);

   /* If we're here, things went ok: */
   return true;
}

/* Wrap the underlying test to be sure that it does not /accidentally/ pass when
it shouldn't. For example, here we want to run the same test whether or not the
target wire protocol version is supposed to support OP_MSG, but it should
already FAIL before it reaches this point; we are performing a final check,
which is to say that it should never pass when the right features aren't
present: */
void
test_mongoc_hello_client_pool ()
{
   // Check with the default protocol version (which here may come from the
   // environment):
   ASSERT (_test_mongoc_should_check_hello () &&
           _test_mongoc_hello_client_pool_impl (-1));

   // Check with a specific protocol version:
   ASSERT (_test_mongoc_should_check_hello () &&
           _test_mongoc_hello_client_pool_impl (MONGOC_SERVER_API_V1));
}

void
test_hello_install (TestSuite *suite)
{
   TestSuite_AddMockServerTest (suite, "/hello", test_mongoc_hello),
      TestSuite_AddMockServerTest (
         suite, "/hello/client_pool", test_mongoc_hello_client_pool);
}
