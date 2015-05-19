#include <mongoc.h>

#include "mongoc-client-private.h"
#include "mongoc-cluster-private.h"
#include "mongoc-topology-private.h"
#include "mongoc-topology-scanner-private.h"
#include "mongoc-tests.h"
#include "TestSuite.h"

#include "test-libmongoc.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "topology-test"

#ifdef _WIN32
static void
usleep (int64_t usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * usec);

    timer = CreateWaitableTimer(NULL, true, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}

#endif
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
   client_a = test_framework_client_new (NULL);
   client_b = test_framework_client_new (NULL);
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
   pool = test_framework_client_pool_new (NULL);
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

   client = test_framework_client_new (NULL);
   assert (client);

   td = &client->topology->description;

   /* call explicitly */
   id = mongoc_cluster_preselect (&client->cluster, MONGOC_OPCODE_QUERY, NULL, &error);
   sd = mongoc_set_get(td->servers, id);
   assert (sd);
   assert (sd->type == MONGOC_SERVER_STANDALONE);

   mongoc_topology_invalidate_server (client->topology, id);
   sd = mongoc_set_get(td->servers, id);
   assert (sd);
   assert (sd->type == MONGOC_SERVER_UNKNOWN);

   fake_sd = bson_malloc0 (sizeof (*fake_sd));

   /* insert a 'fake' server description and ensure that it is invalidated by driver */
   mongoc_server_description_init (fake_sd, "fakeaddress:27033", fake_id);
   fake_sd->type = MONGOC_SERVER_STANDALONE;
   mongoc_set_add(td->servers, fake_id, fake_sd);

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
   pool = test_framework_client_pool_new (NULL);
   client = mongoc_client_pool_pop (pool);
   cluster = &client->cluster;

   usleep(100000);

   /* load stream into cluster */
   id = mongoc_cluster_preselect (cluster, MONGOC_OPCODE_QUERY, NULL, &error);
   cluster_node = mongoc_set_get (cluster->nodes, id);
   scanner_node = mongoc_topology_scanner_get_node (client->topology->scanner, id);
   assert (cluster_node);
   assert (scanner_node);
   assert (cluster_node->stream);
   assert (cluster_node->timestamp > scanner_node->timestamp);

   /* update the scanner node's timestamp */
   usleep(100000);
   scanner_node->timestamp = bson_get_monotonic_time ();
   assert (cluster_node->timestamp < scanner_node->timestamp);
   usleep(100000);

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
   int r;

   /* connect directly and add our user, test is only valid with auth */
   client = test_framework_client_new (NULL);
   database = mongoc_client_get_database(client, "test");
   mongoc_database_remove_user (database, "pink", &error);
   r = mongoc_database_add_user (database, "pink", "panther", NULL, NULL, &error);
   ASSERT_CMPINT(r, ==, 1);
   mongoc_database_destroy (database);
   mongoc_client_destroy (client);

   /* use client pool, test is only valid when multi-threaded */
   pool = test_framework_client_pool_new (NULL);
   client = mongoc_client_pool_pop (pool);

   /* load stream into cluster */
   id = mongoc_cluster_preselect (&client->cluster, MONGOC_OPCODE_QUERY, NULL, &error);

   /* "disconnect": invalidate timestamp and reset server description */
   scanner_node = mongoc_topology_scanner_get_node (client->topology->scanner, id);
   assert (scanner_node);
   scanner_node->timestamp = bson_get_monotonic_time ();
   sd = mongoc_set_get (client->topology->description.servers, id);
   assert (sd);
   mongoc_server_description_reset (sd);

   /* call fetch_stream, ensure that we can still auth with cached wire version */
   stream = mongoc_cluster_fetch_stream (&client->cluster, id, &error);
   assert (stream);

   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
}

void
test_topology_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Topology/client_creation", test_topology_client_creation);
   TestSuite_Add (suite, "/Topology/client_pool_creation", test_topology_client_pool_creation);
   TestSuite_Add (suite, "/Topology/invalidate_server", test_topology_invalidate_server);
   TestSuite_Add (suite, "/Topology/invalid_cluster_node", test_invalid_cluster_node);
   TestSuite_Add (suite, "/Topology/max_wire_version_race_condition", test_max_wire_version_race_condition);
}
