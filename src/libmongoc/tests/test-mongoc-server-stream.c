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

#include "mock_server/mock-server.h"
#include "mock_server/request.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "mongoc.h"
#include "mongoc-client-private.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include "TestSuite.h"

#define HELLO_SERVER_ONE                  \
   tmp_str ("{'ok': 1,"                   \
            " 'isWritablePrimary': true," \
            " 'minWireVersion': %d, "     \
            " 'maxWireVersion': %d }",    \
            WIRE_VERSION_MIN,             \
            WIRE_VERSION_MIN)

#define HELLO_SERVER_TWO                  \
   tmp_str ("{'ok': 1,"                   \
            " 'isWritablePrimary': true," \
            " 'minWireVersion': %d,"      \
            " 'maxWireVersion': %d }",    \
            WIRE_VERSION_MIN,             \
            WIRE_VERSION_MIN + 1)

/* Test that a connection uses the server description from the handshake when
 * checking wire version (instead of the server description from the topology
 * description). */
static void
test_server_stream_ties_server_description_pooled (void *unused)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client_one;
   mongoc_client_t *client_two;
   mongoc_uri_t *uri;
   mock_server_t *server;
   request_t *request;
   future_t *future;
   bson_error_t error;
   mongoc_server_description_t *sd;

   server = mock_server_new ();
   mock_server_run (server);
   uri = mongoc_uri_copy (mock_server_get_uri (server));
   pool = mongoc_client_pool_new (uri);
   client_one = mongoc_client_pool_pop (pool);
   client_two = mongoc_client_pool_pop (pool);

   /* Respond to the monitoring legacy hello with server one hello. */
   request = mock_server_receives_legacy_hello (server, NULL);
   mock_server_replies_simple (request, HELLO_SERVER_ONE);
   request_destroy (request);

   /* Create a connection on client_one. */
   future = future_client_command_simple (client_one,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL /* read prefs */,
                                          NULL /* reply */,
                                          &error);
   /* The first command on a pooled client creates a new connection. */
   request = mock_server_receives_legacy_hello (server, NULL);
   mock_server_replies_simple (request, HELLO_SERVER_ONE);
   request_destroy (request);
   request = mock_server_receives_msg (
      server, MONGOC_MSG_NONE, tmp_bson ("{'$db': 'admin', 'ping': 1}"));
   mock_server_replies_ok_and_destroys (request);
   ASSERT_OR_PRINT (future_get_bool (future), error);
   future_destroy (future);

   /* Create a connection on client_two. */
   future = future_client_command_simple (client_two,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL /* read prefs */,
                                          NULL /* reply */,
                                          &error);
   /* The first command on a pooled client creates a new connection. */
   request = mock_server_receives_legacy_hello (server, NULL);
   mock_server_replies_simple (request, HELLO_SERVER_TWO);
   request_destroy (request);
   request = mock_server_receives_msg (
      server, MONGOC_MSG_NONE, tmp_bson ("{'$db': 'admin', 'ping': 1}"));
   mock_server_replies_ok_and_destroys (request);
   ASSERT_OR_PRINT (future_get_bool (future), error);
   future_destroy (future);

   /* Check that selecting the server returns the second server */
   sd = mongoc_client_select_server (
      client_two, true /* for writes */, NULL /* read prefs */, &error);
   ASSERT_OR_PRINT (sd, error);
   ASSERT_MATCH (mongoc_server_description_hello_response (sd),
                 tmp_str ("{'maxWireVersion': %d}", WIRE_VERSION_MIN + 1));
   mongoc_server_description_destroy (sd);

   mock_server_destroy (server);
   mongoc_uri_destroy (uri);
   mongoc_client_pool_push (pool, client_one);
   mongoc_client_pool_push (pool, client_two);
   mongoc_client_pool_destroy (pool);
}

/* Test that a connection uses the server description from the handshake when
 * checking wire version (instead of the server description from the topology
 * description). */
static void
test_server_stream_ties_server_description_single (void *unused)
{
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   mock_server_t *server;
   request_t *request;
   future_t *future;
   bson_error_t error;
   mongoc_server_description_t *sd;
   mc_tpld_modification tdmod;

   server = mock_server_new ();
   mock_server_run (server);
   uri = mongoc_uri_copy (mock_server_get_uri (server));
   client = mongoc_client_new_from_uri (uri);

   /* Create a connection on client. */
   future = future_client_command_simple (client,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL /* read prefs */,
                                          NULL /* reply */,
                                          &error);
   /* The first command on a client creates a new connection. */
   request = mock_server_receives_legacy_hello (server, NULL);
   mock_server_replies_simple (request, HELLO_SERVER_TWO);
   request_destroy (request);
   request = mock_server_receives_msg (
      server, MONGOC_MSG_NONE, tmp_bson ("{'$db': 'admin', 'ping': 1}"));
   mock_server_replies_ok_and_destroys (request);
   ASSERT_OR_PRINT (future_get_bool (future), error);
   future_destroy (future);

   /* Muck with the topology description. */
   /* Pass in a zeroed out error. */
   memset (&error, 0, sizeof (bson_error_t));
   tdmod = mc_tpld_modify_begin (client->topology);
   mongoc_topology_description_handle_hello (
      tdmod.new_td, 1, tmp_bson (HELLO_SERVER_ONE), 0, &error);
   mc_tpld_modify_commit (tdmod);

   future = future_client_command_simple (client,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL /* read prefs */,
                                          NULL /* reply */,
                                          &error);
   request = mock_server_receives_msg (
      server, MONGOC_MSG_NONE, tmp_bson ("{'$db': 'admin', 'ping': 1}"));
   mock_server_replies_ok_and_destroys (request);
   ASSERT_OR_PRINT (future_get_bool (future), error);
   future_destroy (future);

   /* Check that selecting the server returns the first server */
   sd = mongoc_client_select_server (
      client, true /* for writes */, NULL /* read prefs */, &error);
   ASSERT_OR_PRINT (sd, error);
   ASSERT_MATCH (mongoc_server_description_hello_response (sd),
                 tmp_str ("{'maxWireVersion': %d}", WIRE_VERSION_MIN));
   mongoc_server_description_destroy (sd);

   mock_server_destroy (server);
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
}


void
test_server_stream_install (TestSuite *suite)
{
   TestSuite_AddFull (suite,
                      "/server_stream/ties_server_description/pooled",
                      test_server_stream_ties_server_description_pooled,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      NULL);
   TestSuite_AddFull (suite,
                      "/server_stream/ties_server_description/single",
                      test_server_stream_ties_server_description_single,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      NULL);
}
