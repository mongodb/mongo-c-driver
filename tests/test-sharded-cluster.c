#include <mongoc.h>

#include "mongoc-tests.h"
#include "ha-test.h"

static ha_sharded_cluster_t *cluster;
static ha_replica_set_t *repl_1;
static ha_replica_set_t *repl_2;
static ha_node_t *node_1_1;
static ha_node_t *node_1_2;
static ha_node_t *node_1_3;
static ha_node_t *node_2_1;
static ha_node_t *node_2_2;
static ha_node_t *node_2_3;

static void
test1 (void)
{
}

int
main (int argc,
      char *argv[])
{
   repl_1 = ha_replica_set_new("shardtest1");
   node_1_1 = ha_replica_set_add_replica(repl_1, "shardtest1_1");
   node_1_2 = ha_replica_set_add_replica(repl_1, "shardtest1_2");
   node_1_3 = ha_replica_set_add_replica(repl_1, "shardtest1_3");

   repl_2 = ha_replica_set_new("shardtest2");
   node_2_1 = ha_replica_set_add_replica(repl_2, "shardtest2_1");
   node_2_2 = ha_replica_set_add_replica(repl_2, "shardtest2_2");
   node_2_3 = ha_replica_set_add_replica(repl_2, "shardtest2_3");

   cluster = ha_sharded_cluster_new();
   ha_sharded_cluster_add_replica_set(cluster, repl_1);
   ha_sharded_cluster_add_replica_set(cluster, repl_2);
   ha_sharded_cluster_add_config(cluster, "config1");

   ha_sharded_cluster_start(cluster);
   ha_sharded_cluster_wait_for_healthy(cluster);

   run_test("/ShardedCluster/basic", test1);

   ha_sharded_cluster_shutdown(cluster);

   return 0;
}
