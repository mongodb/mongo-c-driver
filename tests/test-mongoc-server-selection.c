#include <mongoc.h>

#include <stdio.h>

#include "json-test.h"

#include "mongoc-server-description.h"
#include "mongoc-topology-description.h"
#include "mongoc-sdam-private.h"

#include "TestSuite.h"

static mongoc_ss_optype_t
optype_from_test(const char *op)
{
   if (strcmp(op, "read") == 0) {
      return MONGOC_SS_READ;
   } else if (strcmp(op, "write") == 0) {
      return MONGOC_SS_WRITE;
   } else {
      assert(0);
   }
}

static mongoc_read_mode_t
read_mode_from_test(const char *mode)
{
   if (strcmp(mode, "Primary") == 0) {
      return MONGOC_READ_PRIMARY;
   } else if (strcmp(mode, "PrimaryPreferred") == 0) {
      return MONGOC_READ_PRIMARY_PREFERRED;
   } else if (strcmp(mode, "Secondary") == 0) {
      return MONGOC_READ_SECONDARY;
   } else if (strcmp(mode, "SecondaryPreferred") == 0) {
      return MONGOC_READ_SECONDARY_PREFERRED;
   } else if (strcmp(mode, "Nearest") == 0) {
      return MONGOC_READ_NEAREST;
   } else {
      assert(0);
   }
}

static mongoc_topology_description_type_t
topology_status_from_test(const char *type)
{
   if (strcmp(type, "ReplicaSetWithPrimary") == 0) {
      return MONGOC_TOPOLOGY_RS_WITH_PRIMARY;
   } else if (strcmp(type, "ReplicaSetNoPrimary") == 0) {
      return MONGOC_TOPOLOGY_RS_NO_PRIMARY;
   } else if (strcmp(type, "Unknown") == 0) {
      return MONGOC_TOPOLOGY_UNKNOWN;
   } else if (strcmp(type, "Single") == 0) {
      return MONGOC_TOPOLOGY_SINGLE;
   } else if (strcmp(type, "Sharded") == 0) {
      return MONGOC_TOPOLOGY_SHARDED;
   } else {
      printf("can't parse this: %s", type);
      assert(0);
   }
}

static mongoc_server_description_type_t
server_status_from_test(const char *type)
{
   if (strcmp(type, "RSPrimary") == 0) {
      return MONGOC_SERVER_RS_PRIMARY;
   } else if (strcmp(type, "RSSecondary") == 0) {
      return MONGOC_SERVER_RS_SECONDARY;
   } else if (strcmp(type, "Standalone") == 0) {
      return MONGOC_SERVER_STANDALONE;
   } else if (strcmp(type, "Mongos") == 0) {
      return MONGOC_SERVER_MONGOS;
   } else if (strcmp(type, "PossiblePrimary") == 0) {
      return MONGOC_SERVER_POSSIBLE_PRIMARY;
   } else if (strcmp(type, "RSArbiter") == 0) {
      return MONGOC_SERVER_RS_ARBITER;
   } else if (strcmp(type, "RSOther") == 0) {
      return MONGOC_SERVER_RS_OTHER;
   } else if (strcmp(type, "RSGhost") == 0) {
      return MONGOC_SERVER_RS_GHOST;
   } else {
      assert(0);
   }
}

/*
 *-----------------------------------------------------------------------
 *
 * test_rtt_calculation --
 *
 *       Runs the JSON tests for RTT calculation included with the
 *       Server Selection spec.
 *
 *-----------------------------------------------------------------------
 */
static void
test_rtt_calculation (void)
{
   mongoc_server_description_t *description;
   bson_t *test;
   bson_iter_t iter;
   char test_paths[100][MAX_NAME_LENGTH];
   int num_tests;
   int i;

   /* find the JSON files, import and convert to BSON */
   num_tests = collect_tests_from_dir(&test_paths[0],
                                      "tests/json/server_selection/rtt",
                                      0, 100);
   printf("\tfound %d JSON tests\n", num_tests);

   description = bson_malloc0(sizeof *description);
   _mongoc_server_description_init(description, "localhost:27017", 1);

   for (i = 0; i < num_tests; i++) {
      printf("\t\t%s: ", test_paths[i]);
      test = get_bson_from_json_file(test_paths[i]);

      if (test) {
         /* parse RTT into server description */
         assert(bson_iter_init_find(&iter, test, "avg_rtt_ms"));
         description->round_trip_time = bson_iter_int64(&iter);

         /* update server description with new rtt */
         assert(bson_iter_init_find(&iter, test, "new_rtt_ms"));
         _mongoc_server_description_update_rtt(description, bson_iter_int64(&iter));

         /* ensure new RTT was calculated correctly */
         assert(bson_iter_init_find(&iter, test, "new_avg_rtt"));
         assert(description->round_trip_time == bson_iter_int64(&iter));
         printf("PASS\n");
      }
      else {
         printf("NO DATA\n");
      }
   }
   _mongoc_server_description_destroy(description);
}

/*
 *-----------------------------------------------------------------------
 *
 * test_server_selection_logic --
 *
 *      Runs the JSON tests for server selection logic that are
 *      included with the Server Selection spec.
 *
 *-----------------------------------------------------------------------
 */
static void
test_server_selection_logic (void)
{
   mongoc_topology_description_t *topology;
   mongoc_server_description_t *sd;
   mongoc_read_prefs_t *read_prefs;
   mongoc_ss_optype_t op;
   char test_paths[100][MAX_NAME_LENGTH];
   bson_iter_t iter;
   bson_iter_t topology_iter;
   bson_iter_t server_iter;
   bson_iter_t sd_iter;
   bson_iter_t read_pref_iter;
   bson_t *test;
   bson_t test_topology;
   bson_t test_servers;
   bson_t server;
   bson_t candidates;
   bson_t eligible;
   bson_t suitable;
   bson_t latency;
   bson_t test_read_pref;
   bson_t test_tags;
   const uint8_t *data;
   uint32_t len;
   int num_tests;
   int i;
   int j = 0;

   /* import the JSON files and parse into JSON */
   num_tests = collect_tests_from_dir(&test_paths[0],
                                      "tests/json/server_selection/server_selection",
                                      0, 100);

   printf("\tfound %d JSON tests\n", num_tests);

   for (i = 0; i < num_tests; i++) {
      printf("\t\t%s: ", test_paths[i]);
      test = get_bson_from_json_file(test_paths[i]);

      if (test) {
         topology = bson_malloc0(sizeof *topology);
         _mongoc_topology_description_init(topology, NULL);

         /* pull out topology description field */
         assert(bson_iter_init_find(&iter, test, "topology_description"));
         bson_iter_document(&iter, &len, &data);
         assert(bson_init_static(&test_topology, data, len));

         /* set topology state from test */
         assert(bson_iter_init_find(&topology_iter, &test_topology, "type"));
         topology->type = topology_status_from_test(bson_iter_utf8(&topology_iter, NULL));

         /* for each server description in test, add server to our topology */
         assert(bson_iter_init_find(&topology_iter, &test_topology, "servers"));
         bson_iter_array (&topology_iter, &len, &data);
         assert(bson_init_static (&test_servers, data, len));

         bson_iter_init(&server_iter, &test_servers);
         while (bson_iter_next (&server_iter)) {
            bson_iter_document(&server_iter, &len, &data);
            assert(bson_init_static(&server, data, len));

            /* initialize new server description with given address */
            sd = bson_malloc0(sizeof *sd);
            assert(bson_iter_init_find(&sd_iter, &server, "address"));
            _mongoc_server_description_init(sd, bson_iter_utf8(&sd_iter, NULL), j++);

            /* set description rtt */
            assert(bson_iter_init_find(&sd_iter, &server, "avg_rtt_ms"));
            sd->round_trip_time = bson_iter_int32(&sd_iter);

            /* set description type */
            assert(bson_iter_init_find(&sd_iter, &server, "type"));
            sd->type = server_status_from_test(bson_iter_utf8(&sd_iter, NULL));

            /* set description tags */
            assert(bson_iter_init_find(&sd_iter, &server, "tags"));
            bson_iter_array(&sd_iter, &len, &data);
            assert(bson_init_static(&sd->tags, data, len));

            /* add new server to our topology description */
            mongoc_set_add(topology->servers, sd->id, sd);
         }

         /* create read preference document from test */
         assert (bson_iter_init_find(&iter, test, "read_preference"));
         bson_iter_document(&iter, &len, &data);
         assert(bson_init_static(&test_read_pref, data, len));

         assert (bson_iter_init_find(&read_pref_iter, &test_read_pref, "mode"));
         read_prefs = mongoc_read_prefs_new(read_mode_from_test(bson_iter_utf8(&read_pref_iter, NULL)));

         assert (bson_iter_init_find(&read_pref_iter, &test_read_pref, "tags"));
         bson_iter_array(&read_pref_iter, &len, &data);
         assert(bson_init_static(&test_tags, data, len));
         mongoc_read_prefs_set_tags(read_prefs, &test_tags);

         /* read in candidate servers */
         assert (bson_iter_init_find(&iter, test, "candidate_servers"));
         bson_iter_array (&iter, &len, &data);
         assert (bson_init_static (&candidates, data, len));

         /* read in eligible servers */
         assert (bson_iter_init_find(&iter, test, "eligible_servers"));
         bson_iter_array (&iter, &len, &data);
         assert (bson_init_static (&eligible, data, len));

         /* read in suitable servers */
         assert (bson_iter_init_find(&iter, test, "suitable_servers"));
         bson_iter_array (&iter, &len, &data);
         assert (bson_init_static (&suitable, data, len));

         /* read in latency window servers */
         assert (bson_iter_init_find(&iter, test, "in_latency_window"));
         bson_iter_array (&iter, &len, &data);
         assert (bson_init_static (&latency, data, len));

         /* get optype */
         assert (bson_iter_init_find(&iter, test, "operation"));
         op = optype_from_test(bson_iter_utf8(&iter, NULL));

         /* send through server selection and make sure we calculate correctly */
         // TODO, once SS is implemented in discrete steps

         _mongoc_topology_description_destroy(topology);
         printf("PASS\n");
      }
      else {
         printf("NOT RUN\n");
      }
   }
}


void
test_server_selection_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/ServerSelection/rtt", test_rtt_calculation);
   TestSuite_Add (suite, "/ServerSelection/logic", test_server_selection_logic);
}
