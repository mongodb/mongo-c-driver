#include <string.h>
#include <unistd.h>

#include "ha-test.h"

#include "mongoc-client.h"
#include "mongoc-client-private.h"
#include "mongoc-cluster-private.h"
#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-tests.h"


static ha_replica_set_t *replica_set;
static ha_node_t *r1;
static ha_node_t *r2;
static ha_node_t *r3;
static ha_node_t *a1;


static void
insert_test_docs (mongoc_collection_t *collection)
{
   bson_error_t error;
   bson_oid_t oid;
   bson_t b;
   int i;

   for (i = 0; i < 200; i++) {
      bson_init(&b);
      bson_oid_init(&oid, NULL);
      bson_append_oid(&b, "_id", 3, &oid);
      if (!mongoc_collection_insert(collection,
                                    MONGOC_INSERT_NONE,
                                    &b,
                                    NULL,
                                    &error)) {
         MONGOC_ERROR("%s", error.message);
         abort();
      }
      bson_destroy(&b);
   }
}


static ha_node_t *
get_replica (mongoc_cluster_node_t *node)
{
   ha_node_t *iter;

   for (iter = replica_set->nodes; iter; iter = iter->next) {
      if (iter->port == node->host.port) {
         return iter;
      }
   }

   BSON_ASSERT(FALSE);

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * test1 --
 *
 *       Tests the failover scenario of a node having a network partition
 *       between the time the client recieves the first OP_REPLY and the
 *       submission of a followup OP_GETMORE.
 *
 *       This function will abort() upon failure.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
test1 (void)
{
   mongoc_cluster_node_t *node;
   mongoc_collection_t *collection;
   mongoc_read_prefs_t *read_prefs;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   const bson_t *doc;
   bson_bool_t r;
   ha_node_t *replica;
   bson_t q;
   int i;

   bson_init(&q);

   client = ha_replica_set_create_client(replica_set);
   collection = mongoc_client_get_collection(client, "test1", "test1");

   MONGOC_DEBUG("Inserting test documents.");
   insert_test_docs(collection);
   MONGOC_INFO("Test documents inserted.");

   read_prefs = mongoc_read_prefs_new(MONGOC_READ_SECONDARY);

   MONGOC_DEBUG("Sending query to a SECONDARY.");
   cursor = mongoc_collection_find(collection,
                                   MONGOC_QUERY_NONE,
                                   0,
                                   100,
                                   &q,
                                   NULL,
                                   read_prefs);

   BSON_ASSERT(cursor);
   BSON_ASSERT(!cursor->hint);

   /*
    * Send OP_QUERY to server and get first document back.
    */
   MONGOC_INFO("Sending OP_QUERY.");
   r = mongoc_cursor_next(cursor, &doc);
   BSON_ASSERT(r);
   BSON_ASSERT(cursor->hint);
   BSON_ASSERT(cursor->sent);
   BSON_ASSERT(!cursor->done);
   BSON_ASSERT(!cursor->end_of_event);

   /*
    * Exhaust the items in our first OP_REPLY.
    */
   MONGOC_DEBUG("Exhausting OP_REPLY.");
   for (i = 0; i < 98; i++) {
      r = mongoc_cursor_next(cursor, &doc);
      BSON_ASSERT(r);
      BSON_ASSERT(cursor->hint);
      BSON_ASSERT(!cursor->done);
      BSON_ASSERT(!cursor->end_of_event);
   }

   /*
    * Finish off the last item in this OP_REPLY.
    */
   MONGOC_INFO("Fetcing last doc from OP_REPLY.");
   r = mongoc_cursor_next(cursor, &doc);
   BSON_ASSERT(r);
   BSON_ASSERT(cursor->hint);
   BSON_ASSERT(cursor->sent);
   BSON_ASSERT(!cursor->done);
   BSON_ASSERT(cursor->end_of_event);

   /*
    * Determine which node we queried by using the hint to
    * get the cluster information.
    */
   BSON_ASSERT(cursor->hint);
   node = &client->cluster.nodes[cursor->hint - 1];
   replica = get_replica(node);

   /*
    * Kill the node we are communicating with.
    */
   MONGOC_INFO("Killing replicaSet node to synthesize failure.");
   ha_node_kill(replica);

   /*
    * Try to fetch the next result set, expect failure.
    */
   MONGOC_DEBUG("Checking for expected failure.");
   r = mongoc_cursor_next(cursor, &doc);
   BSON_ASSERT(!r);

   mongoc_cursor_destroy(cursor);
   mongoc_read_prefs_destroy(read_prefs);
   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
   bson_destroy(&q);
}


/*
 *--------------------------------------------------------------------------
 *
 * main --
 *
 *       Test various replica-set failure scenarios.
 *
 * Returns:
 *       0 on success; otherwise context specific error code.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int
main (int   argc,   /* IN */
      char *argv[]) /* IN */
{
   replica_set = ha_replica_set_new("test1");
   r1 = ha_replica_set_add_replica(replica_set, "replica1");
   r2 = ha_replica_set_add_replica(replica_set, "replica2");
   r3 = ha_replica_set_add_replica(replica_set, "replica3");
   a1 = ha_replica_set_add_arbiter(replica_set, "arbiter1");

   ha_replica_set_start(replica_set);
   ha_replica_set_wait_for_healthy(replica_set);

   run_test("/ReplicaSet/lose_node_during_cursor", test1);

   ha_replica_set_shutdown(replica_set);
   ha_replica_set_destroy(replica_set);

   return 0;
}
