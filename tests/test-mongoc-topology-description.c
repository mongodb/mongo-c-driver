#include "mongoc.h"
#include "mongoc-client-pool-private.h"
#include "mongoc-client-private.h"

#include "TestSuite.h"
#include "test-libmongoc.h"
#include "test-conveniences.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "topology-test"

static void
_test_has_readable_writable_server (bool pooled)
{
   mongoc_client_t *client;
   mongoc_client_pool_t *pool = NULL;
   mongoc_topology_description_t *td;
   mongoc_read_prefs_t *prefs;
   bool r;
   bson_error_t error;

   if (pooled) {
      pool = test_framework_client_pool_new ();
      client = mongoc_client_pool_pop (pool);
      td = _mongoc_client_pool_get_topology_description (pool);
   } else {
      client = test_framework_client_new ();
      td = &client->topology->description;
   }

   prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   mongoc_read_prefs_set_tags (prefs, tmp_bson ("[{'tag': 'does-not-exist'}]"));

   /* not yet connected */
   ASSERT (!mongoc_topology_description_has_writable_server (td));
   ASSERT (!mongoc_topology_description_has_readable_server (td, NULL));
   ASSERT (!mongoc_topology_description_has_readable_server (td, prefs));

   /* trigger connection */
   r = mongoc_client_command_simple (client, "admin", tmp_bson ("{'ping': 1}"),
                                     NULL, NULL, &error);
   ASSERT_OR_PRINT (r, error);

   ASSERT (mongoc_topology_description_has_writable_server (td));
   ASSERT (mongoc_topology_description_has_readable_server (td, NULL));

   if (test_framework_is_replset ()) {
      /* prefs still don't match any server */
      ASSERT (!mongoc_topology_description_has_readable_server (td, prefs));
   } else {
      /* topology type single ignores read preference */
      ASSERT (mongoc_topology_description_has_readable_server (td, prefs));
   }

   mongoc_read_prefs_destroy (prefs);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }
}


static void
test_has_readable_writable_server_single (void)
{
   _test_has_readable_writable_server (false);
}


static void
test_has_readable_writable_server_pooled (void)
{
   _test_has_readable_writable_server (true);
}


void
test_topology_description_install (TestSuite *suite)
{
   TestSuite_AddLive (suite, "/TopologyDescription/readable_writable/single",
                      test_has_readable_writable_server_single);
   TestSuite_AddLive (suite, "/TopologyDescription/readable_writable/pooled",
                      test_has_readable_writable_server_pooled);
}
