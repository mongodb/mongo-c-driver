#include <mongoc.h>

#include "mongoc-client-private.h"
#include "mongoc-cluster-private.h"
#include "mongoc-set-private.h"
#include "mongoc-tests.h"
#include "TestSuite.h"

#include "test-libmongoc.h"

static char *gTestUri;

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
   mongoc_uri_t *uri;
   int32_t max_bson_obj_size = 16;
   uint32_t id;

   /* single-threaded */
   client = mongoc_client_new (gTestUri);
   assert (client);

   /* with given server */
   id = mongoc_cluster_preselect (&client->cluster, MONGOC_OPCODE_QUERY, NULL, &error);
   sd = mongoc_set_get (client->topology->description.servers, id);
   sd->max_bson_obj_size = max_bson_obj_size;
   assert (max_bson_obj_size = mongoc_cluster_get_max_bson_obj_size (&client->cluster, &id));

   /* with no given server */
   assert (max_bson_obj_size = mongoc_cluster_get_max_bson_obj_size (&client->cluster, NULL));

   mongoc_client_destroy (client);

   /* multi-threaded */
   uri = mongoc_uri_new (gTestUri);
   pool = mongoc_client_pool_new (uri);
   client = mongoc_client_pool_pop (pool);

   /* with given server */
   id = mongoc_cluster_preselect (&client->cluster, MONGOC_OPCODE_QUERY, NULL, &error);
   node = mongoc_set_get (client->cluster.nodes, id);
   node->max_bson_obj_size = max_bson_obj_size;
   assert (max_bson_obj_size = mongoc_cluster_get_max_bson_obj_size (&client->cluster, &id));

   /* without given server */
   assert (max_bson_obj_size = mongoc_cluster_get_max_bson_obj_size (&client->cluster, NULL));

   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
   mongoc_uri_destroy (uri);
}

static void
test_get_max_msg_size (void)
{
   mongoc_server_description_t *sd;
   mongoc_cluster_node_t *node;
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_uri_t *uri;
   int32_t max_msg_size = 32;
   uint32_t id;

   /* single-threaded */
   client = mongoc_client_new (gTestUri);

   /* with given server */
   id = mongoc_cluster_preselect (&client->cluster, MONGOC_OPCODE_QUERY, NULL, &error);
   sd = mongoc_set_get (client->topology->description.servers, id);
   sd->max_msg_size = max_msg_size;
   assert (max_msg_size = mongoc_cluster_get_max_msg_size (&client->cluster, &id));

   /* with no given server */
   assert (max_msg_size = mongoc_cluster_get_max_msg_size (&client->cluster, NULL));

   mongoc_client_destroy (client);

   /* multi-threaded */
   uri = mongoc_uri_new (gTestUri);
   pool = mongoc_client_pool_new (uri);
   client = mongoc_client_pool_pop (pool);

   /* with given server */
   id = mongoc_cluster_preselect (&client->cluster, MONGOC_OPCODE_QUERY, NULL, &error);
   node = mongoc_set_get (client->cluster.nodes, id);
   node->max_msg_size = max_msg_size;
   assert (max_msg_size = mongoc_cluster_get_max_msg_size (&client->cluster, &id));

   /* without given server */
   assert (max_msg_size = mongoc_cluster_get_max_msg_size (&client->cluster, NULL));

   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
   mongoc_uri_destroy (uri);
}

static void
cleanup_globals (void)
{
   bson_free (gTestUri);
}

void
test_cluster_install (TestSuite *suite)
{
   gTestUri = bson_strdup_printf("mongodb://%s/", MONGOC_TEST_HOST);

   TestSuite_Add (suite, "/Cluster/test_get_max_bson_obj_size", test_get_max_bson_obj_size);
   TestSuite_Add (suite, "/Cluster/test_get_max_msg_size", test_get_max_msg_size);

   atexit (cleanup_globals);
}
