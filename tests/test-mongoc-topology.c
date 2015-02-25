#include <mongoc.h>

#include "mongoc-client-private.h"
#include "mongoc-topology-private.h"
#include "mongoc-tests.h"
#include "TestSuite.h"

#include "test-libmongoc.h"

static char *gTestUri;

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "topology-test"

static void
test_topology_client_creation (void)
{
   mongoc_topology_t *topology_a;
   mongoc_topology_t *topology_b;
   mongoc_client_t *client_a;
   mongoc_client_t *client_b;

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

   // TODO: ensure that they share sockets with the client
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

   atexit (cleanup_globals);
}
