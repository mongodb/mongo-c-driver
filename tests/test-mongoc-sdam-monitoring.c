#include <mongoc.h>

#include "json-test.h"

#include "mongoc-client-private.h"

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif


/*
 *-----------------------------------------------------------------------
 *
 * Run the JSON tests from the SDAM Monitoring spec.
 *
 *-----------------------------------------------------------------------
 */
static void
test_sdam_monitoring_cb (bson_t *test)
{
   mongoc_server_description_t *sd;
   mongoc_client_t *client;
   bson_error_t error;
   bson_t ismasters;
   bson_t phase;
   bson_t phases;
   bson_t ismaster;
   bson_t response;
   bson_t servers;
   bson_t server;
   bson_t outcome;
   bson_iter_t phase_iter;
   bson_iter_t phase_field_iter;
   bson_iter_t ismaster_iter;
   bson_iter_t ismaster_field_iter;
   bson_iter_t servers_iter;
   bson_iter_t outcome_iter;
   bson_iter_t iter;
   const char *set_name;
   const char *hostname;

   /* parse out the uri and use it to create a client */
   assert (bson_iter_init_find(&iter, test, "uri"));
   client = mongoc_client_new(bson_iter_utf8(&iter, NULL));

   /* for each phase, parse and validate */
   assert (bson_iter_init_find(&iter, test, "phases"));
   bson_iter_bson (&iter, &phases);
   bson_iter_init (&phase_iter, &phases);

   while (bson_iter_next (&phase_iter)) {
      bson_iter_bson (&phase_iter, &phase);

      /* grab ismaster responses out and feed them to topology */
      assert (bson_iter_init_find(&phase_field_iter, &phase, "responses"));
      bson_iter_bson (&phase_field_iter, &ismasters);
      bson_iter_init (&ismaster_iter, &ismasters);

      while (bson_iter_next (&ismaster_iter)) {
         bson_iter_bson (&ismaster_iter, &ismaster);

         /* fetch server description for this server based on its hostname */
         bson_iter_init_find (&ismaster_field_iter, &ismaster, "0");
         sd = server_description_by_hostname (&client->topology->description,
                                              bson_iter_utf8(&ismaster_field_iter, NULL));

         /* if server has been removed from topology, skip */
         if (!sd) continue;

         bson_iter_init_find (&ismaster_field_iter, &ismaster, "1");
         bson_iter_bson (&ismaster_field_iter, &response);

         /* send ismaster through the topology description's handler */
         mongoc_topology_description_handle_ismaster(&client->topology->description,
                                                     sd,
                                                     &response,
                                                     15,
                                                     &error);
      }

      /* parse out "outcome" and validate */
      assert (bson_iter_init_find(&phase_field_iter, &phase, "outcome"));
      bson_iter_bson (&phase_field_iter, &outcome);
      bson_iter_init (&outcome_iter, &outcome);

      while (bson_iter_next (&outcome_iter)) {
         fprintf (stderr, "ERROR: unparsed test field %s\n", bson_iter_key (&outcome_iter));
         /*assert (false);*/
      }
   }

   mongoc_client_destroy (client);
}

/*
 *-----------------------------------------------------------------------
 *
 * Runner for the JSON tests for SDAM Monitoring..
 *
 *-----------------------------------------------------------------------
 */
static void
test_all_spec_tests (TestSuite *suite)
{
   char resolved[PATH_MAX];

   ASSERT (realpath ("tests/json/server_discovery_and_monitoring/monitoring",
                     resolved));

   install_json_test_suite (suite, resolved, &test_sdam_monitoring_cb);
}

void
test_sdam_monitoring_install (TestSuite *suite)
{
   test_all_spec_tests (suite);
}
