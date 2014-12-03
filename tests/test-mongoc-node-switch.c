#include <mongoc.h>

#include "mongoc-node-switch-private.h"

#include "TestSuite.h"

static void
test_node_switch_new (void)
{
   mongoc_stream_t *streams[10];
   int i;

   mongoc_node_switch_t * ns = mongoc_node_switch_new();

   for (i = 0; i < 5; i++) {
      streams[i] = mongoc_stream_file_new_for_path (BINARY_DIR "/insert1.dat", O_RDONLY, 0);
      mongoc_node_switch_add(ns, i, streams[i]);
   }

   for (i = 0; i < 5; i++) {
      assert( mongoc_node_switch_get(ns, i) == streams[i]);
   }

   mongoc_node_switch_rm(ns, 0);

   for (i = 5; i < 10; i++) {
      streams[i] = mongoc_stream_file_new_for_path (BINARY_DIR "/insert1.dat", O_RDONLY, 0);
      mongoc_node_switch_add(ns, i, streams[i]);
   }

   for (i = 5; i < 10; i++) {
      assert( mongoc_node_switch_get(ns, i) == streams[i]);
   }

   mongoc_node_switch_rm(ns, 9);
   mongoc_node_switch_rm(ns, 5);

   assert( mongoc_node_switch_get(ns, 1) == streams[1]);
   assert( mongoc_node_switch_get(ns, 7) == streams[7]);
   assert( ! mongoc_node_switch_get(ns, 5) );

   mongoc_node_switch_destroy(ns);
}


void
test_node_switch_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/NodeSwitch/new", test_node_switch_new);
}
