#include <mongoc.h>
#include <mongoc-uri-private.h>

#include "mongoc-client-private.h"
#include "mongoc-tests.h"
#include "mongoc-util-private.h"
#include "TestSuite.h"

#include "test-libmongoc.h"
#include "mock_server/mock-server.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "topology-test"

static int should_run_topology_tests (void)
{
   if (getenv ("FOREVER_GREEN")) {
      return 0;
   }
   return 1;
}

static void
test_topology_client_creation (void)
{
   mongoc_uri_t *uri;
   mongoc_topology_scanner_node_t *node;
   mongoc_topology_t *topology_a;
   mongoc_topology_t *topology_b;
   mongoc_client_t *client_a;
   mongoc_client_t *client_b;
   mongoc_stream_t *topology_stream;
   mongoc_stream_t *cluster_stream;
   bson_error_t error;
   uint32_t id;

   uri = test_framework_get_uri ();
   mongoc_uri_set_option_as_int32 (uri, "connectTimeoutMS", 12345);
   mongoc_uri_set_option_as_int32 (uri, "serverSelectionTimeoutMS", 54321);

   /* create two clients directly */
   client_a = mongoc_client_new_from_uri (uri);
   client_b = mongoc_client_new_from_uri (uri);
   assert (client_a);
   assert (client_b);

   /* ensure that they are using different topologies */
   topology_a = client_a->topology;
   topology_b = client_b->topology;
   assert (topology_a);
   assert (topology_b);
   assert (topology_a != topology_b);

   assert (topology_a->connect_timeout_msec == 12345);
   assert (topology_a->server_selection_timeout_msec == 54321);

   /* ensure that their topologies are running in single-threaded mode */
   assert (topology_a->single_threaded);
   assert (topology_a->bg_thread_state == MONGOC_TOPOLOGY_BG_OFF);

   /* ensure that we are sharing streams with the client */
   id = mongoc_cluster_preselect (&client_a->cluster, MONGOC_OPCODE_QUERY, NULL, &error);
   cluster_stream = mongoc_cluster_fetch_stream (&client_a->cluster, id, &error);
   node = mongoc_topology_scanner_get_node (client_a->topology->scanner, id);
   assert (node);
   topology_stream = node->stream;
   assert (topology_stream);
   assert (topology_stream == cluster_stream);

   mongoc_client_destroy (client_a);
   mongoc_client_destroy (client_b);
   mongoc_uri_destroy (uri);
}

static void
test_topology_client_pool_creation (void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client_a;
   mongoc_client_t *client_b;
   mongoc_topology_t *topology_a;
   mongoc_topology_t *topology_b;

   /* create two clients through a client pool */
   pool = test_framework_client_pool_new ();
   client_a = mongoc_client_pool_pop (pool);
   client_b = mongoc_client_pool_pop (pool);
   assert (client_a);
   assert (client_b);

   /* ensure that they are using the same topology */
   topology_a = client_a->topology;
   topology_b = client_b->topology;
   assert (topology_a);
   assert (topology_a == topology_b);

   /* ensure that that topology is running in a background thread */
   assert (!topology_a->single_threaded);
   assert (topology_a->bg_thread_state != MONGOC_TOPOLOGY_BG_OFF);

   mongoc_client_pool_push (pool, client_a);
   mongoc_client_pool_push (pool, client_b);
   mongoc_client_pool_destroy (pool);
}

static void
test_server_selection_try_once_option (void)
{
   const char *uri_strings[3] = {
      "mongodb://a",
      "mongodb://a/?serverSelectionTryOnce=true",
      "mongodb://a/?serverSelectionTryOnce=false" };

   int i;
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   mongoc_client_pool_t *pool;

   /* try_once is on by default for non-pooled, can be turned off */
   client = mongoc_client_new (uri_strings[0]);
   assert (client->topology->server_selection_try_once);
   mongoc_client_destroy (client);

   client = mongoc_client_new (uri_strings[1]);
   assert (client->topology->server_selection_try_once);
   mongoc_client_destroy (client);

   client = mongoc_client_new (uri_strings[2]);
   assert (! client->topology->server_selection_try_once);
   mongoc_client_destroy (client);
   
   /* off for pooled clients, can't be enabled */
   for (i = 0; i < sizeof (uri_strings) / sizeof (char *); i++) {
      uri = mongoc_uri_new ("mongodb://a");
      pool = mongoc_client_pool_new (uri);
      client = mongoc_client_pool_pop (pool);
      assert (!client->topology->server_selection_try_once);
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
      mongoc_uri_destroy (uri);
   }
}

static void
_test_server_selection (bool try_once)
{
   mock_server_t *server;
   char *secondary_response;
   char *primary_response;
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_read_prefs_t *primary_pref;
   future_t *future;
   bson_error_t error;
   request_t *request;
   mongoc_server_description_t *sd;

   server = mock_server_new ();
   mock_server_set_request_timeout_msec (server, 600);
   mock_server_run (server);

   secondary_response = bson_strdup_printf (
      "{'ok': 1, "
      " 'ismaster': false,"
      " 'secondary': true,"
      " 'setName': 'rs',"
      " 'hosts': ['%s']}",
      mock_server_get_host_and_port (server));

   primary_response = bson_strdup_printf (
      "{'ok': 1, "
      " 'ismaster': true,"
      " 'setName': 'rs',"
      " 'hosts': ['%s']}",
      mock_server_get_host_and_port (server));

   uri = mongoc_uri_copy (mock_server_get_uri (server));
   mongoc_uri_set_option_as_utf8 (uri, "replicaSet", "rs");
   mongoc_uri_set_option_as_int32 (uri, "heartbeatFrequencyMS", 500);
   mongoc_uri_set_option_as_int32 (uri, "serverSelectionTimeoutMS", 100);
   if (!try_once) {
      /* serverSelectionTryOnce is on by default */
      mongoc_uri_set_option_as_bool (uri, "serverSelectionTryOnce", false);
   }

   client = mongoc_client_new_from_uri (uri);
   primary_pref = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   /* no primary, selection fails after one try */
   future = future_topology_select (client->topology, MONGOC_SS_READ,
                                    primary_pref, 15, &error);
   assert (request = mock_server_receives_ismaster (server));
   mock_server_replies_simple (request, secondary_response);
   request_destroy (request);

   /* the selection timeout is 100 ms, and we can't rescan until a half second
    * passes, so selection fails without another ismaster call */
   assert (!mock_server_receives_ismaster (server));

   /* selection fails */
   assert (!future_get_mongoc_server_description_ptr (future));
   ASSERT_CMPINT (error.domain, ==, MONGOC_ERROR_SERVER_SELECTION);
   ASSERT_CMPINT (error.code, ==, MONGOC_ERROR_SERVER_SELECTION_FAILURE);

   if (try_once) {
      ASSERT_CMPSTR ("No suitable servers found", error.message);
   } else {
      ASSERT_CMPSTR ("Timed out trying to select a server", error.message);
   }

   assert (client->topology->stale);
   future_destroy (future);

   _mongoc_usleep (510 * 1000);  /* one heartbeat, plus a few milliseconds */

   /* second selection, now we try ismaster again */
   future = future_topology_select (client->topology, MONGOC_SS_READ,
                                    primary_pref, 15, &error);
   assert (request = mock_server_receives_ismaster (server));

   /* the secondary is now primary, selection succeeds */
   mock_server_replies_simple (request, primary_response);
   sd = future_get_mongoc_server_description_ptr (future);
   assert (sd);
   assert (!client->topology->stale);
   request_destroy (request);
   future_destroy (future);

   mongoc_server_description_destroy (sd);
   mongoc_read_prefs_destroy (primary_pref);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   bson_free (secondary_response);
   bson_free (primary_response);
   mock_server_destroy (server);
}

static void
test_server_selection_try_once (void *context)
{
   _test_server_selection (true);
}

static void
test_server_selection_try_once_false (void *context)
{
   _test_server_selection (false);
}

static void
test_topology_invalidate_server (void)
{
   mongoc_server_description_t *fake_sd;
   mongoc_server_description_t *sd;
   mongoc_topology_description_t *td;
   mongoc_rpc_t rpc;
   mongoc_buffer_t buffer;
   mongoc_client_t *client;
   bson_error_t error;
   uint32_t fake_id = 42;
   uint32_t id;

   client = test_framework_client_new ();
   assert (client);

   td = &client->topology->description;

   /* call explicitly */
   id = mongoc_cluster_preselect (&client->cluster, MONGOC_OPCODE_QUERY, NULL, &error);
   sd = (mongoc_server_description_t *)mongoc_set_get(td->servers, id);
   assert (sd);
   assert (sd->type == MONGOC_SERVER_STANDALONE ||
           sd->type == MONGOC_SERVER_RS_PRIMARY ||
           sd->type == MONGOC_SERVER_MONGOS);

   mongoc_topology_invalidate_server (client->topology, id);
   sd = (mongoc_server_description_t *)mongoc_set_get(td->servers, id);
   assert (sd);
   assert (sd->type == MONGOC_SERVER_UNKNOWN);

   fake_sd = (mongoc_server_description_t *)bson_malloc0 (sizeof (*fake_sd));

   /* insert a 'fake' server description and ensure that it is invalidated by driver */
   mongoc_server_description_init (fake_sd, "fakeaddress:27033", fake_id);
   fake_sd->type = MONGOC_SERVER_STANDALONE;
   mongoc_set_add(td->servers, fake_id, fake_sd);

   /* with recv */
   _mongoc_buffer_init(&buffer, NULL, 0, NULL, NULL);
   _mongoc_client_recv(client, &rpc, &buffer, fake_id, &error);
   sd = (mongoc_server_description_t *)mongoc_set_get(td->servers, fake_id);
   assert (sd);
   assert (sd->type == MONGOC_SERVER_UNKNOWN);

   /* with recv_gle */
   sd->type = MONGOC_SERVER_STANDALONE;
   _mongoc_client_recv_gle(client, fake_id, NULL, &error);
   sd = (mongoc_server_description_t *)mongoc_set_get(td->servers, fake_id);
   assert (sd);
   assert (sd->type == MONGOC_SERVER_UNKNOWN);

   _mongoc_buffer_destroy (&buffer);

   mongoc_client_destroy (client);
}

static void
test_invalid_cluster_node (void)
{
   mongoc_client_pool_t *pool;
   mongoc_cluster_node_t *cluster_node;
   mongoc_topology_scanner_node_t *scanner_node;
   bson_error_t error;
   mongoc_client_t *client;
   mongoc_cluster_t *cluster;
   uint32_t id;

   /* use client pool, this test is only valid when multi-threaded */
   pool = test_framework_client_pool_new ();
   client = mongoc_client_pool_pop (pool);
   cluster = &client->cluster;

   _mongoc_usleep (100 * 1000);;

   /* load stream into cluster */
   id = mongoc_cluster_preselect (cluster, MONGOC_OPCODE_QUERY, NULL, &error);
   cluster_node = (mongoc_cluster_node_t *)mongoc_set_get (cluster->nodes, id);
   scanner_node = mongoc_topology_scanner_get_node (client->topology->scanner, id);
   assert (cluster_node);
   assert (scanner_node);
   assert (cluster_node->stream);
   assert (cluster_node->timestamp > scanner_node->timestamp);

   /* update the scanner node's timestamp */
   _mongoc_usleep (100 * 1000);;
   scanner_node->timestamp = bson_get_monotonic_time ();
   assert (cluster_node->timestamp < scanner_node->timestamp);
   _mongoc_usleep (100 * 1000);;

   /* ensure that cluster adjusts */
   mongoc_cluster_fetch_stream (cluster, id, &error);
   assert (cluster_node->timestamp > scanner_node->timestamp);

   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
}

static void
test_max_wire_version_race_condition (void)
{
   mongoc_topology_scanner_node_t *scanner_node;
   mongoc_server_description_t *sd;
   mongoc_database_t *database;
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_stream_t *stream;
   bson_error_t error;
   uint32_t id;

   /* connect directly and add our user, test is only valid with auth */
   client = test_framework_client_new ();
   database = mongoc_client_get_database(client, "test");
   mongoc_database_remove_user (database, "pink", &error);
   ASSERT_OR_PRINT (1 == mongoc_database_add_user (
      database, "pink", "panther", NULL, NULL, &error), error);
   mongoc_database_destroy (database);
   mongoc_client_destroy (client);

   /* use client pool, test is only valid when multi-threaded */
   pool = test_framework_client_pool_new ();
   client = mongoc_client_pool_pop (pool);

   /* load stream into cluster */
   id = mongoc_cluster_preselect (&client->cluster, MONGOC_OPCODE_QUERY, NULL, &error);

   /* "disconnect": invalidate timestamp and reset server description */
   scanner_node = mongoc_topology_scanner_get_node (client->topology->scanner, id);
   assert (scanner_node);
   scanner_node->timestamp = bson_get_monotonic_time ();
   sd = (mongoc_server_description_t *)mongoc_set_get (client->topology->description.servers, id);
   assert (sd);
   mongoc_server_description_reset (sd);

   /* call fetch_stream, ensure that we can still auth with cached wire version */
   stream = mongoc_cluster_fetch_stream (&client->cluster, id, &error);
   assert (stream);

   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
}


static void
test_cooldown_standalone (void)
{
   mock_server_t *server;
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_read_prefs_t *primary_pref;
   future_t *future;
   bson_error_t error;
   request_t *request;
   mongoc_server_description_t *sd;

   server = mock_server_new ();
   mock_server_set_request_timeout_msec (server, 100);
   mock_server_run (server);
   uri = mongoc_uri_copy (mock_server_get_uri (server));
   /* anything less than minHeartbeatFrequencyMS=500 is irrelevant */
   mongoc_uri_set_option_as_int32 (uri, "serverSelectionTimeoutMS", 100);
   client = mongoc_client_new_from_uri (uri);
   primary_pref = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   /* first ismaster fails, selection fails */
   future = future_topology_select (client->topology, MONGOC_SS_READ,
                                    primary_pref, 15, &error);
   assert (request = mock_server_receives_ismaster (server));
   mock_server_hangs_up (request);
   assert (!future_get_mongoc_server_description_ptr (future));
   request_destroy (request);
   future_destroy (future);

   _mongoc_usleep (1000 * 1000);  /* 1 second */

   /* second selection doesn't try to call ismaster: we're in cooldown */
   future = future_topology_select (client->topology, MONGOC_SS_READ,
                                    primary_pref, 15, &error);
   assert (!mock_server_receives_ismaster (server));  /* no ismaster call */
   assert (!future_get_mongoc_server_description_ptr (future));
   future_destroy (future);

   _mongoc_usleep (5100 * 1000);  /* 5.1 seconds */

   /* cooldown ends, now we try ismaster again, this time succeeding */
   future = future_topology_select (client->topology, MONGOC_SS_READ,
                                    primary_pref, 15, &error);
   request = mock_server_receives_ismaster (server);  /* not in cooldown now */
   assert (request);
   mock_server_replies_simple (request, "{'ok': 1, 'ismaster': true}");
   sd = future_get_mongoc_server_description_ptr (future);
   assert (sd);
   request_destroy (request);
   future_destroy (future);

   mongoc_server_description_destroy (sd);
   mongoc_read_prefs_destroy (primary_pref);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   mock_server_destroy (server);
}


static void
test_cooldown_rs (void)
{
   mock_server_t *servers[2];  /* two secondaries, no primary */
   char *uri_str;
   int i;
   mongoc_client_t *client;
   mongoc_read_prefs_t *primary_pref;
   char *secondary_response;
   char *primary_response;
   future_t *future;
   bson_error_t error;
   request_t *request;
   mongoc_server_description_t *sd;

   for (i = 0; i < 2; i++) {
      servers[i] = mock_server_new ();
      mock_server_set_request_timeout_msec (servers[i], 600);
      mock_server_run (servers[i]);
   }

   uri_str = bson_strdup_printf (
      "mongodb://localhost:%hu/?replicaSet=rs"
         "&serverSelectionTimeoutMS=100"
         "&connectTimeoutMS=100",
      mock_server_get_port (servers[0]));

   client = mongoc_client_new (uri_str);
   primary_pref = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   secondary_response = bson_strdup_printf (
      "{'ok': 1, 'ismaster': false, 'secondary': true, 'setName': 'rs',"
      " 'hosts': ['localhost:%hu', 'localhost:%hu']}",
      mock_server_get_port (servers[0]),
      mock_server_get_port (servers[1]));

   primary_response = bson_strdup_printf (
      "{'ok': 1, 'ismaster': true, 'setName': 'rs',"
      " 'hosts': ['localhost:%hu', 'localhost:%hu']}",
      mock_server_get_port (servers[0]),
      mock_server_get_port (servers[1]));

   /* server 0 is a secondary. */
   future = future_topology_select (client->topology, MONGOC_SS_READ,
                                    primary_pref, 15, &error);

   assert (request = mock_server_receives_ismaster (servers[0]));
   mock_server_replies_simple (request, secondary_response);
   request_destroy (request);

   /* server 0 told us about server 1. we check it immediately but it's down. */
   assert (request = mock_server_receives_ismaster (servers[1]));
   mock_server_hangs_up (request);
   request_destroy (request);

   /* selection fails. */
   assert (!future_get_mongoc_server_description_ptr (future));
   future_destroy (future);

   _mongoc_usleep (1000 * 1000);  /* 1 second */

   /* second selection doesn't try ismaster on server 1: it's in cooldown */
   future = future_topology_select (client->topology, MONGOC_SS_READ,
                                    primary_pref, 15, &error);

   assert (request = mock_server_receives_ismaster (servers[0]));
   mock_server_replies_simple (request, secondary_response);
   request_destroy (request);

   assert (!mock_server_receives_ismaster (servers[1]));  /* no ismaster call */

   /* still no primary */
   assert (!future_get_mongoc_server_description_ptr (future));
   future_destroy (future);

   _mongoc_usleep (5100 * 1000);  /* 5.1 seconds */

   /* cooldown ends, now we try ismaster on server 1, this time succeeding */
   future = future_topology_select (client->topology, MONGOC_SS_READ,
                                    primary_pref, 15, &error);

   request = mock_server_receives_ismaster (servers[1]);
   assert (request);
   mock_server_replies_simple (request, primary_response);
   request_destroy (request);

   /* server 0 doesn't need to respond */
   sd = future_get_mongoc_server_description_ptr (future);
   assert (sd);
   future_destroy (future);

   mongoc_server_description_destroy (sd);
   mongoc_read_prefs_destroy (primary_pref);
   mongoc_client_destroy (client);
   bson_free (secondary_response);
   bson_free (primary_response);
   bson_free (uri_str);
   mock_server_destroy (servers[0]);
}


static void
_test_connect_timeout (bool pooled, bool try_once)
{
   const int32_t connect_timeout_ms = 50;
   const int32_t server_selection_timeout_ms = 10 * 1000;  /* 10 seconds */

   mock_server_t *servers[2];
   int i;
   char *secondary_response;
   char *uri_str;
   mongoc_uri_t *uri;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   mongoc_read_prefs_t *primary_pref;
   future_t *future;
   int64_t start;
   int64_t duration_usec;
   bson_error_t error;
   request_t *request;
   int n_loops;

   assert (!(pooled && try_once));  /* not supported */

   for (i = 0; i < 2; i++) {
      servers[i] = mock_server_new ();
      mock_server_run (servers[i]);
   };

   secondary_response = bson_strdup_printf ("{'ok': 1,"
                                            " 'ismaster': false,"
                                            " 'secondary': true,"
                                            " 'setName': 'rs'}");

   uri_str = bson_strdup_printf (
      "mongodb://localhost:%hu,localhost:%hu/"
         "?replicaSet=rs&connectTimeoutMS=%d&serverSelectionTimeoutMS=%d",
      mock_server_get_port (servers[0]),
      mock_server_get_port (servers[1]),
      connect_timeout_ms,
      server_selection_timeout_ms);

   uri = mongoc_uri_new (uri_str);
   assert (uri);

   if (!pooled && !try_once) {
      /* override default */
      mongoc_uri_set_option_as_bool (uri, "serverSelectionTryOnce", false);
   }

   if (pooled) {
      pool = mongoc_client_pool_new (uri);
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (uri);
   }

   primary_pref = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   /* start waiting for a server */
   future = future_topology_select (client->topology, MONGOC_SS_READ,
                                    primary_pref, 15, &error);

   start = bson_get_monotonic_time ();

   /* server 0 doesn't respond */
   assert (request = mock_server_receives_ismaster (servers[0]));
   request_destroy (request);

   /* server 1 is a secondary */
   request = mock_server_receives_ismaster (servers[1]);
   mock_server_replies_simple (request, secondary_response);
   request_destroy (request);

   if (!try_once) {
      /* driver retries every minHeartbeatFrequencyMS + connectTimeoutMS */
      n_loops = server_selection_timeout_ms / (500 + connect_timeout_ms);

      for (i = 1; i <= n_loops; i++) {
         request = mock_server_receives_ismaster (servers[1]);
         mock_server_replies_simple (request, secondary_response);
         request_destroy (request);

         duration_usec = bson_get_monotonic_time () - start;
         ASSERT_ALMOST_EQUAL (duration_usec / 1000,
                              i * (500 + connect_timeout_ms));

         /* single client puts server 0 in cooldown for 5 sec */
         if (pooled || i == 10) {
            assert (request = mock_server_receives_ismaster (servers[0]));
            request_destroy (request);  /* don't respond */
         }
      }
   }

   /* selection fails */
   assert (!future_get_mongoc_server_description_ptr (future));
   future_destroy (future);

   duration_usec = bson_get_monotonic_time () - start;

   if (try_once) {
      ASSERT_ALMOST_EQUAL (duration_usec / 1000, connect_timeout_ms);
   } else {
      ASSERT_ALMOST_EQUAL (duration_usec / 1000, server_selection_timeout_ms);
   }

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   mongoc_read_prefs_destroy (primary_pref);
   mongoc_uri_destroy (uri);
   bson_free (uri_str);
   bson_free (secondary_response);

   for (i = 0; i < 2; i++) {
      mock_server_destroy (servers[i]);
   };
}


static void
test_connect_timeout_pooled (void *context)
{
   _test_connect_timeout (true, false);
}


static void
test_connect_timeout_single(void *context)
{
   _test_connect_timeout (false, true);
}


static void
test_connect_timeout_try_once_false(void *context)
{
   _test_connect_timeout (false, false);
}


void
test_topology_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Topology/client_creation", test_topology_client_creation);
   TestSuite_Add (suite, "/Topology/client_pool_creation", test_topology_client_pool_creation);
   TestSuite_Add (suite, "/Topology/server_selection_try_once_option", test_server_selection_try_once_option);
   TestSuite_AddFull (suite, "/Topology/server_selection_try_once", test_server_selection_try_once, NULL, NULL, should_run_topology_tests);
   TestSuite_AddFull (suite, "/Topology/server_selection_try_once_false", test_server_selection_try_once_false, NULL, NULL, should_run_topology_tests);
   TestSuite_Add (suite, "/Topology/invalidate_server", test_topology_invalidate_server);
   TestSuite_Add (suite, "/Topology/invalid_cluster_node", test_invalid_cluster_node);
   TestSuite_Add (suite, "/Topology/max_wire_version_race_condition", test_max_wire_version_race_condition);
   TestSuite_Add (suite, "/Topology/cooldown/standalone", test_cooldown_standalone);
   TestSuite_Add (suite, "/Topology/cooldown/rs", test_cooldown_rs);
   TestSuite_AddFull (suite, "/Topology/connect_timeout/pooled", test_connect_timeout_pooled, NULL, NULL, should_run_topology_tests);
   TestSuite_AddFull (suite, "/Topology/connect_timeout/single/try_once", test_connect_timeout_single, NULL, NULL, should_run_topology_tests);
   TestSuite_AddFull (suite, "/Topology/connect_timeout/single/try_once_false", test_connect_timeout_try_once_false, NULL, NULL, should_run_topology_tests);
}
