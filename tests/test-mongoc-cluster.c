#include <mongoc.h>

#include "mongoc-client-private.h"

#include "mongoc-tests.h"
#include "TestSuite.h"
#include "test-libmongoc.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cluster-test"

static void
test_get_max_bson_obj_size (void)
{
   mongoc_server_description_t *sd;
   mongoc_cluster_node_t *node;
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   bson_error_t error;
   int32_t max_bson_obj_size = 16;
   uint32_t id;

   /* single-threaded */
   client = test_framework_client_new ();
   assert (client);

   id = mongoc_cluster_preselect (&client->cluster, MONGOC_OPCODE_QUERY, NULL, &error);
   sd = (mongoc_server_description_t *)mongoc_set_get (client->topology->description.servers, id);
   sd->max_bson_obj_size = max_bson_obj_size;
   assert (max_bson_obj_size == mongoc_cluster_get_max_bson_obj_size (&client->cluster));

   mongoc_client_destroy (client);

   /* multi-threaded */
   pool = test_framework_client_pool_new ();
   client = mongoc_client_pool_pop (pool);

   ASSERT_OR_PRINT (id = mongoc_cluster_preselect (&client->cluster,
                                                   MONGOC_OPCODE_QUERY,
                                                   NULL,
                                                   &error), error);

   node = (mongoc_cluster_node_t *)mongoc_set_get (client->cluster.nodes, id);
   node->max_bson_obj_size = max_bson_obj_size;
   assert (max_bson_obj_size = mongoc_cluster_get_max_bson_obj_size (&client->cluster));

   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
}

static void
test_get_max_msg_size (void)
{
   mongoc_server_description_t *sd;
   mongoc_cluster_node_t *node;
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   bson_error_t error;
   int32_t max_msg_size = 32;
   uint32_t id;

   /* single-threaded */
   client = test_framework_client_new ();

   ASSERT_OR_PRINT (id = mongoc_cluster_preselect (&client->cluster,
                                                   MONGOC_OPCODE_QUERY,
                                                   NULL,
                                                   &error), error);

   sd = (mongoc_server_description_t *)mongoc_set_get (client->topology->description.servers, id);
   sd->max_msg_size = max_msg_size;
   assert (max_msg_size == mongoc_cluster_get_max_msg_size (&client->cluster));

   mongoc_client_destroy (client);

   /* multi-threaded */
   pool = test_framework_client_pool_new ();
   client = mongoc_client_pool_pop (pool);

   ASSERT_OR_PRINT (id = mongoc_cluster_preselect (&client->cluster,
                                                   MONGOC_OPCODE_QUERY,
                                                   NULL,
                                                   &error), error);

   node = (mongoc_cluster_node_t *)mongoc_set_get (client->cluster.nodes, id);
   node->max_msg_size = max_msg_size;
   assert (max_msg_size == mongoc_cluster_get_max_msg_size (&client->cluster));

   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
}

void
test_cluster_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Cluster/test_get_max_bson_obj_size", test_get_max_bson_obj_size);
   TestSuite_Add (suite, "/Cluster/test_get_max_msg_size", test_get_max_msg_size);
}
