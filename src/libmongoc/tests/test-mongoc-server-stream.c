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

#include <mongoc/mcd-rpc.h>
#include <mongoc/mongoc.h>

#include <TestSuite.h>
#include <mock_server/future-functions.h>
#include <mock_server/future.h>
#include <mock_server/mock-server.h>
#include <mock_server/request.h>
#include <test-conveniences.h>
#include <test-libmongoc.h>

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "test_cluster"

/* A mongos just below the exhaust cursor threshold (wire 21 = MongoDB 7.0). */
#define HELLO_SERVER_ONE                    \
   tmp_str("{'ok': 1,"                      \
           " 'isWritablePrimary': true,"    \
           " 'msg': 'isdbgrid',"            \
           " 'minWireVersion': %d, "        \
           " 'maxWireVersion': %d }",       \
           WIRE_VERSION_MONGOS_EXHAUST - 1, \
           WIRE_VERSION_MONGOS_EXHAUST - 1)

/* A mongos at the exhaust cursor threshold (wire 22 = MongoDB 7.1). */
#define HELLO_SERVER_TWO                 \
   tmp_str("{'ok': 1,"                   \
           " 'isWritablePrimary': true," \
           " 'msg': 'isdbgrid',"         \
           " 'minWireVersion': %d,"      \
           " 'maxWireVersion': %d }",    \
           WIRE_VERSION_MIN,             \
           WIRE_VERSION_MONGOS_EXHAUST)

/* run_exhaust_find runs a find with the "exhaust" option. The exhaust gate
 * reads the server description pinned on the connection's server stream.
 *
 * If @expect_error is true, expect a client-side error from a maxWireVersion <
 * WIRE_VERSION_MONGOS_EXHAUST. The server does not receive anything. */
static void
run_exhaust_find(bool expect_error, mongoc_client_t *client, mock_server_t *server)
{
   mongoc_collection_t *coll;
   mongoc_cursor_t *cursor;
   const bson_t *doc = NULL;
   bson_error_t error = {0};
   future_t *future;
   request_t *request;

   ASSERT(client);

   coll = mongoc_client_get_collection(client, "db", "coll");

   cursor = mongoc_collection_find_with_opts(coll, tmp_bson("{}"), tmp_bson("{'exhaust': true}"), NULL);

   future = future_cursor_next(cursor, &doc);

   if (expect_error) {
      /* Expect a client side error. The server does not receive anything. */
      ASSERT(!future_get_bool(future));
      ASSERT(mongoc_cursor_error(cursor, &error));
      ASSERT_ERROR_CONTAINS(error,
                            MONGOC_ERROR_CURSOR,
                            MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                            "exhaust cursors require mongos with wire version: 21");
   } else {
      /* The exhaust find is sent with the exhaust-allowed OP_MSG flag. */
      request = mock_server_receives_msg(server, MONGOC_OP_MSG_FLAG_EXHAUST_ALLOWED, tmp_bson("{'find': 'coll'}"));
      reply_to_request_simple(request, "{'cursor': {'id': 0, 'ns': 'db.coll', 'firstBatch': []}, 'ok': 1}");
      /* No documents, but no error either. */
      ASSERT(!future_get_bool(future));
      ASSERT(!mongoc_cursor_error(cursor, &error));
   }

   future_destroy(future);
   mongoc_cursor_destroy(cursor);
   mongoc_collection_destroy(coll);
}

/* Test that a connection uses the server description from the handshake when
 * checking wire version (instead of the server description from the topology
 * description). */
static void
test_server_stream_ties_server_description_pooled(void *unused)
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

   BSON_UNUSED(unused);

   server = mock_server_new();
   mock_server_run(server);
   uri = mongoc_uri_copy(mock_server_get_uri(server));
   pool = test_framework_client_pool_new_from_uri(uri, NULL);
   client_one = mongoc_client_pool_pop(pool);
   client_two = mongoc_client_pool_pop(pool);

   /* The exhaust cursor gate requires a sharded topology. The mock is a
    * single-seed mongos (topology type "Single" by seed count), so set the
    * topology type directly; the SDAM transition table only upgrades
    * "Unknown" to "Sharded". */
   {
      mc_tpld_modification tdmod = mc_tpld_modify_begin(client_one->topology);
      tdmod.new_td->type = MONGOC_TOPOLOGY_SHARDED;
      mc_tpld_modify_commit(tdmod);
   }

   /* Respond to the monitoring hello with server one hello. */
   request = mock_server_receives_any_hello(server);
   reply_to_request_simple(request, HELLO_SERVER_ONE);
   request_destroy(request);

   /* Create a connection on client_one. */
   future = future_client_command_simple(
      client_one, "admin", tmp_bson("{'ping': 1}"), NULL /* read prefs */, NULL /* reply */, &error);
   /* The first command on a pooled client creates a new connection. */
   request = mock_server_receives_any_hello(server);
   reply_to_request_simple(request, HELLO_SERVER_ONE);
   request_destroy(request);
   request = mock_server_receives_msg(server, MONGOC_MSG_NONE, tmp_bson("{'$db': 'admin', 'ping': 1}"));
   reply_to_request_with_ok_and_destroy(request);
   ASSERT_OR_PRINT(future_get_bool(future), error);
   future_destroy(future);

   /* Create a connection on client_two. */
   future = future_client_command_simple(
      client_two, "admin", tmp_bson("{'ping': 1}"), NULL /* read prefs */, NULL /* reply */, &error);
   /* The first command on a pooled client creates a new connection. */
   request = mock_server_receives_any_hello(server);
   reply_to_request_simple(request, HELLO_SERVER_TWO);
   request_destroy(request);
   request = mock_server_receives_msg(server, MONGOC_MSG_NONE, tmp_bson("{'$db': 'admin', 'ping': 1}"));
   reply_to_request_with_ok_and_destroy(request);
   ASSERT_OR_PRINT(future_get_bool(future), error);
   future_destroy(future);

   /* Check that selecting the server returns the second server */
   sd = mongoc_client_select_server(client_two, true /* for writes */, NULL /* read prefs */, &error);
   ASSERT_OR_PRINT(sd, error);
   ASSERT_MATCH(mongoc_server_description_hello_response(sd),
                tmp_str("{'maxWireVersion': %d}", WIRE_VERSION_MONGOS_EXHAUST));
   mongoc_server_description_destroy(sd);

   /* Expect client_one to continue to use maxWireVersion=WIRE_VERSION_MONGOS_EXHAUST - 1
    * for wire version checks (the topology description reports the newer
    * maxWireVersion from client_two's handshake). Expect an error when using an
    * exhaust cursor. */
   run_exhaust_find(true, client_one, server);
   /* Expect client_two to continue to use maxWireVersion=WIRE_VERSION_MONGOS_EXHAUST
    * for wire version checks. Expect no error when using an exhaust cursor. */
   run_exhaust_find(false, client_two, server);

   mongoc_client_pool_push(pool, client_one);
   mongoc_client_pool_push(pool, client_two);
   mongoc_client_pool_destroy(pool);
   mongoc_uri_destroy(uri);
   mock_server_destroy(server);
}

/* Test that a connection uses the server description from the handshake when
 * checking wire version (instead of the server description from the topology
 * description). */
static void
test_server_stream_ties_server_description_single(void *unused)
{
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   mock_server_t *server;
   request_t *request;
   future_t *future;
   bson_error_t error;
   mongoc_server_description_t *sd;
   mc_tpld_modification tdmod;

   BSON_UNUSED(unused);

   server = mock_server_new();
   mock_server_run(server);
   uri = mongoc_uri_copy(mock_server_get_uri(server));
   client = test_framework_client_new_from_uri(uri, NULL);

   /* The exhaust cursor gate requires a sharded topology. The mock is a
    * single-seed mongos (topology type "Single" by seed count), so set the
    * topology type directly; the SDAM transition table only upgrades
    * "Unknown" to "Sharded". */
   {
      mc_tpld_modification type_tdmod = mc_tpld_modify_begin(client->topology);
      type_tdmod.new_td->type = MONGOC_TOPOLOGY_SHARDED;
      mc_tpld_modify_commit(type_tdmod);
   }

   /* Create a connection on client. */
   future = future_client_command_simple(
      client, "admin", tmp_bson("{'ping': 1}"), NULL /* read prefs */, NULL /* reply */, &error);
   /* The first command on a client creates a new connection. */
   request = mock_server_receives_any_hello(server);
   reply_to_request_simple(request, HELLO_SERVER_TWO);
   request_destroy(request);
   request = mock_server_receives_msg(server, MONGOC_MSG_NONE, tmp_bson("{'$db': 'admin', 'ping': 1}"));
   reply_to_request_with_ok_and_destroy(request);
   ASSERT_OR_PRINT(future_get_bool(future), error);
   future_destroy(future);

   /* Muck with the topology description. */
   /* Pass in a zeroed out error. */
   memset(&error, 0, sizeof(bson_error_t));
   tdmod = mc_tpld_modify_begin(client->topology);
   mongoc_topology_description_handle_hello(tdmod.new_td,
                                            &client->topology->log_and_monitor,
                                            1,
                                            tmp_bson(HELLO_SERVER_ONE),
                                            0,
                                            MONGOC_TOPOLOGY_DESCRIPTION_HELLO_CLUSTER_TIME_UPDATE,
                                            &error);
   mc_tpld_modify_commit(tdmod);

   future = future_client_command_simple(
      client, "admin", tmp_bson("{'ping': 1}"), NULL /* read prefs */, NULL /* reply */, &error);
   request = mock_server_receives_msg(server, MONGOC_MSG_NONE, tmp_bson("{'$db': 'admin', 'ping': 1}"));
   reply_to_request_with_ok_and_destroy(request);
   ASSERT_OR_PRINT(future_get_bool(future), error);
   future_destroy(future);

   /* Check that selecting the server returns the first server */
   sd = mongoc_client_select_server(client, true /* for writes */, NULL /* read prefs */, &error);
   ASSERT_OR_PRINT(sd, error);
   ASSERT_MATCH(mongoc_server_description_hello_response(sd),
                tmp_str("{'maxWireVersion': %d}", WIRE_VERSION_MONGOS_EXHAUST - 1));
   mongoc_server_description_destroy(sd);

   /* Expect client to continue to use maxWireVersion=WIRE_VERSION_MONGOS_EXHAUST
    * for wire version checks (the topology description reports the older
    * maxWireVersion). Expect no error when using an exhaust cursor. */
   run_exhaust_find(false, client, server);

   mongoc_client_destroy(client);
   mongoc_uri_destroy(uri);
   mock_server_destroy(server);
}


void
test_server_stream_install(TestSuite *suite)
{
   TestSuite_AddFull(suite,
                     "/server_stream/ties_server_description/pooled",
                     test_server_stream_ties_server_description_pooled,
                     NULL /* dtor */,
                     NULL /* ctx */,
                     NULL);
   TestSuite_AddFull(suite,
                     "/server_stream/ties_server_description/single",
                     test_server_stream_ties_server_description_single,
                     NULL /* dtor */,
                     NULL /* ctx */,
                     NULL);
}
