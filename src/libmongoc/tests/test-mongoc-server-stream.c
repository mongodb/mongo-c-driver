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

#define HELLO_PRE_OPMSG                                         \
   "{'ok': 1, 'isWritablePrimary': true, 'minWireVersion': 0, " \
   "'maxWireVersion': 5 }"
#define HELLO_POST_OPMSG                                        \
   "{'ok': 1, 'isWritablePrimary': true, 'minWireVersion': 0, " \
   "'maxWireVersion': 6 }"

/* Test that a connection uses the server description from the handshake when
 * checking wire version (instead of the server description from the topology
 * description). */
static void
test_server_stream_ties_server_description_pooled (void *unused)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client_opquery;
   mongoc_client_t *client_opmsg;
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
   client_opquery = mongoc_client_pool_pop (pool);
   client_opmsg = mongoc_client_pool_pop (pool);

   /* Respond to the monitoring legacy hello with wire version pre 3.6 (before
    * OP_MSG). */
   request = mock_server_receives_legacy_hello (server, NULL);
   mock_server_replies_simple (request, HELLO_PRE_OPMSG);
   request_destroy (request);

   /* Create a connection on client_opquery. */
   future = future_client_command_simple (client_opquery,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL /* read prefs */,
                                          NULL /* reply */,
                                          &error);
   /* The first command on a pooled client creates a new connection. */
   request = mock_server_receives_legacy_hello (server, NULL);
   mock_server_replies_simple (request, HELLO_PRE_OPMSG);
   request_destroy (request);
   /* Check that the mock server receives an OP_QUERY. */
   request = mock_server_receives_command (
      server, "admin", MONGOC_QUERY_SECONDARY_OK, "{'ping': 1}");
   ASSERT_CMPINT ((int) request->opcode, ==, (int) MONGOC_OPCODE_QUERY);
   mock_server_replies_ok_and_destroys (request);
   ASSERT_OR_PRINT (future_get_bool (future), error);
   future_destroy (future);

   /* Create a connection on client_opmsg. */
   future = future_client_command_simple (client_opmsg,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL /* read prefs */,
                                          NULL /* reply */,
                                          &error);
   /* The first command on a pooled client creates a new connection. */
   request = mock_server_receives_legacy_hello (server, NULL);
   mock_server_replies_simple (request, HELLO_POST_OPMSG);
   request_destroy (request);
   /* Check that the mock server receives an OP_MSG. */
   request = mock_server_receives_msg (server, 0, tmp_bson ("{'ping': 1}"));
   ASSERT_CMPINT ((int) request->opcode, ==, (int) MONGOC_OPCODE_MSG);
   mock_server_replies_ok_and_destroys (request);
   ASSERT_OR_PRINT (future_get_bool (future), error);
   future_destroy (future);

   /* Re-use client_opquery, to ensure it still uses opquery. */
   future = future_client_command_simple (client_opquery,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL /* read prefs */,
                                          NULL /* reply */,
                                          &error);
   request = mock_server_receives_command (
      server, "admin", MONGOC_QUERY_SECONDARY_OK, "{'ping': 1}");
   ASSERT_CMPINT ((int) request->opcode, ==, (int) MONGOC_OPCODE_QUERY);
   mock_server_replies_ok_and_destroys (request);
   ASSERT_OR_PRINT (future_get_bool (future), error);
   future_destroy (future);

   /* Re-use client_opmsg, to ensure it still uses opmsg. */
   future = future_client_command_simple (client_opmsg,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL /* read prefs */,
                                          NULL /* reply */,
                                          &error);
   request = mock_server_receives_msg (server, 0, tmp_bson ("{'ping': 1}"));
   ASSERT_CMPINT ((int) request->opcode, ==, (int) MONGOC_OPCODE_MSG);
   mock_server_replies_ok_and_destroys (request);
   ASSERT_OR_PRINT (future_get_bool (future), error);
   future_destroy (future);

   /* Check that selecting the server still returns the OP_QUERY server */
   sd = mongoc_client_select_server (
      client_opmsg, true /* for writes */, NULL /* read prefs */, &error);
   ASSERT_OR_PRINT (sd, error);
   ASSERT_MATCH (mongoc_server_description_hello_response (sd),
                 "{'maxWireVersion': 6}");
   mongoc_server_description_destroy (sd);

   mock_server_destroy (server);
   mongoc_uri_destroy (uri);
   mongoc_client_pool_push (pool, client_opquery);
   mongoc_client_pool_push (pool, client_opmsg);
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
   mock_server_replies_simple (request, HELLO_POST_OPMSG);
   request_destroy (request);
   /* Check that the mock server receives an OP_MSG. */
   request = mock_server_receives_msg (server, 0, tmp_bson ("{'ping': 1}"));
   ASSERT_CMPINT ((int) request->opcode, ==, (int) MONGOC_OPCODE_MSG);
   mock_server_replies_ok_and_destroys (request);
   ASSERT_OR_PRINT (future_get_bool (future), error);
   future_destroy (future);

   /* Muck with the topology description. */
   /* Pass in a zeroed out error. */
   memset (&error, 0, sizeof (bson_error_t));
   mongoc_topology_description_handle_hello (
      &client->topology->description, 1, tmp_bson (HELLO_PRE_OPMSG), 0, &error);

   /* Send another command, it should still use OP_MSG. */
   future = future_client_command_simple (client,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL /* read prefs */,
                                          NULL /* reply */,
                                          &error);
   request = mock_server_receives_msg (server, 0, tmp_bson ("{'ping': 1}"));
   ASSERT_CMPINT ((int) request->opcode, ==, (int) MONGOC_OPCODE_MSG);
   mock_server_replies_ok_and_destroys (request);
   ASSERT_OR_PRINT (future_get_bool (future), error);
   future_destroy (future);

   /* Check that selecting the server returns the OP_QUERY server */
   sd = mongoc_client_select_server (
      client, true /* for writes */, NULL /* read prefs */, &error);
   ASSERT_OR_PRINT (sd, error);
   ASSERT_MATCH (mongoc_server_description_hello_response (sd),
                 "{'maxWireVersion': 5}");
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
