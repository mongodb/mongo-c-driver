#include <mongoc.h>

#include "mongoc-client-private.h"
#include "mongoc-cluster-private.h"
#include "mongoc-topology-private.h"
#include "mongoc-topology-scanner-private.h"
#include "mongoc-tests.h"
#include "TestSuite.h"

#include "test-libmongoc.h"

static char *gTestUri;

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "topology-test"

static void
test_topology_client_creation (void)
{
   mongoc_topology_scanner_node_t *node;
   mongoc_topology_t *topology_a;
   mongoc_topology_t *topology_b;
   mongoc_client_t *client_a;
   mongoc_client_t *client_b;
   mongoc_stream_t *topology_stream;
   mongoc_stream_t *cluster_stream;
   bson_error_t error;
   uint32_t id;

   /* create two clients directly */
   client_a = mongoc_client_new(gTestUri);
   client_b = mongoc_client_new(gTestUri);
   assert (client_a);
   assert (client_b);

   /* ensure that they are using different topologies */
   topology_a = client_a->topology;
   topology_b = client_b->topology;
   assert (topology_a);
   assert (topology_b);
   assert (topology_a != topology_b);

   /* ensure that their topologies are running in single-threaded mode */
   assert (topology_a->single_threaded);
   assert (topology_a->bg_thread_state == MONGOC_TOPOLOGY_BG_OFF);

   /* ensure that calling background_thread_start or stop does nothing */
   mongoc_topology_background_thread_start(topology_a);
   assert (topology_a->single_threaded);
   assert (topology_a->bg_thread_state == MONGOC_TOPOLOGY_BG_OFF);

   mongoc_topology_background_thread_stop(topology_a);
   assert (topology_a->single_threaded);
   assert (topology_a->bg_thread_state == MONGOC_TOPOLOGY_BG_OFF);

   /* ensure that we are sharing streams with the client */
   id = mongoc_cluster_preselect (&client_a->cluster, MONGOC_OPCODE_QUERY, NULL, NULL, &error);
   cluster_stream = mongoc_cluster_fetch_stream (&client_a->cluster, id, &error);
   node = mongoc_topology_scanner_get_node (client_a->topology->scanner, id);
   assert (node);
   topology_stream = node->stream;
   assert (topology_stream);
   assert (topology_stream == cluster_stream);

   mongoc_client_destroy (client_a);
   mongoc_client_destroy (client_b);
}

static void
test_topology_client_pool_creation (void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client_a;
   mongoc_client_t *client_b;
   mongoc_topology_t *topology_a;
   mongoc_topology_t *topology_b;
   mongoc_uri_t *uri;

   /* create two clients through a client pool */
   uri = mongoc_uri_new (gTestUri);
   pool = mongoc_client_pool_new (uri);
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
   mongoc_uri_destroy (uri);
   mongoc_client_pool_destroy (pool);
}

static void
test_topology_invalidate_server (void)
{
   mongoc_server_description_t fake_sd;
   mongoc_server_description_t *sd;
   mongoc_topology_description_t *td;
   mongoc_rpc_t rpc;
   mongoc_buffer_t buffer;
   mongoc_client_t *client;
   bson_error_t error;
   uint32_t fake_id = 42;
   uint32_t id;

   client = mongoc_client_new(gTestUri);
   assert (client);

   td = &client->topology->description;

   /* call explicitly */
   id = mongoc_cluster_preselect (&client->cluster, MONGOC_OPCODE_QUERY, NULL, NULL, &error);
   sd = mongoc_set_get(td->servers, id);
   assert (sd);
   assert (sd->type == MONGOC_SERVER_STANDALONE);

   mongoc_topology_invalidate_server (client->topology, id);
   sd = mongoc_set_get(td->servers, id);
   assert (sd);
   assert (sd->type == MONGOC_SERVER_UNKNOWN);

   /* insert a 'fake' server description and ensure that it is invalidated by driver */
   mongoc_server_description_init (&fake_sd, "fakeaddress:27033", fake_id);
   fake_sd.type = MONGOC_SERVER_STANDALONE;
   mongoc_set_add(td->servers, fake_id, &fake_sd);

   /* with recv */
   _mongoc_buffer_init(&buffer, NULL, 0, NULL, NULL);
   _mongoc_client_recv(client, &rpc, &buffer, fake_id, &error);
   sd = mongoc_set_get(td->servers, fake_id);
   assert (sd);
   assert (sd->type == MONGOC_SERVER_UNKNOWN);

   /* with recv_gle */
   sd->type = MONGOC_SERVER_STANDALONE;
   _mongoc_client_recv_gle(client, fake_id, NULL, &error);
   sd = mongoc_set_get(td->servers, fake_id);
   assert (sd);
   assert (sd->type == MONGOC_SERVER_UNKNOWN);

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
   mongoc_stream_t *stream;
   mongoc_uri_t *uri;
   uint32_t id;

   /* use client pool, this test is only valid when multi-threaded */
   uri = mongoc_uri_new (gTestUri);
   pool = mongoc_client_pool_new (uri);
   client = mongoc_client_pool_pop (pool);
   cluster = &client->cluster;

   /* load stream into cluster */
   id = mongoc_cluster_preselect (cluster, MONGOC_OPCODE_QUERY, NULL, NULL, &error);
   cluster_node = mongoc_set_get (cluster->nodes, id);
   scanner_node = mongoc_topology_scanner_get_node (client->topology->scanner, id);
   assert (cluster_node);
   assert (scanner_node);
   assert (cluster_node->stream);
   assert (cluster_node->timestamp > scanner_node->timestamp);

   /* update the scanner node's timestamp */
   scanner_node->timestamp = bson_get_monotonic_time ();
   assert (cluster_node->timestamp < scanner_node->timestamp);

   /* ensure that cluster adjusts */
   stream = mongoc_cluster_fetch_stream (cluster, id, &error);
   assert (cluster_node->timestamp > scanner_node->timestamp);

   mongoc_client_pool_push (pool, client);
   mongoc_uri_destroy (uri);
   mongoc_client_pool_destroy (pool);
}

static void
cleanup_globals (void)
{
   bson_free(gTestUri);
}

void
test_topology_install (TestSuite *suite)
{
   gTestUri = bson_strdup_printf("mongodb://%s/", MONGOC_TEST_HOST);

   TestSuite_Add (suite, "/Topology/client_creation", test_topology_client_creation);
   TestSuite_Add (suite, "/Topology/client_pool_creation", test_topology_client_pool_creation);
   TestSuite_Add (suite, "/Topology/invalidate_server", test_topology_invalidate_server);
   TestSuite_Add (suite, "/Topology/invalid_cluster_node", test_invalid_cluster_node);

   atexit (cleanup_globals);
}
