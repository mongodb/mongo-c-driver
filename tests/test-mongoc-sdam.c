#include <mongoc.h>

#include <stdio.h>
#include <mongoc-set-private.h>

#include "json-test.h"

#include "mongoc-client-private.h"
#include "mongoc-server-description-private.h"
#include "mongoc-topology-description-private.h"
#include "mongoc-topology-private.h"

#include "TestSuite.h"

#define MAX_NUM_TESTS 100

#if defined(_WIN32) && !defined(strcasecmp)
# define strcasecmp _stricmp
#endif

/* caller must clean up the returned description */
static mongoc_server_description_t *
_server_description_by_hostname(mongoc_topology_description_t *topology,
                                const char *address)
{
   mongoc_set_t *set = topology->servers;
   mongoc_server_description_t *server_iter;
   int i;

   for (i = 0; i < set->items_len; i++) {
      server_iter = set->items[i].item;
      if (strcasecmp(address, server_iter->connection_address) == 0) {
         return server_iter;
      }
   }
   return NULL;
}

static void
_topology_has_description(mongoc_topology_description_t *topology,
                          bson_t *server,
                          const char *address)
{
   mongoc_server_description_t *sd;
   bson_iter_t server_iter;
   const char *set_name;

   sd = _server_description_by_hostname(topology, address);
   assert (sd);

   bson_iter_init(&server_iter, server);
   while (bson_iter_next (&server_iter)) {
      if (strcmp("setName", bson_iter_key (&server_iter)) == 0) {
         set_name = bson_iter_utf8(&server_iter, NULL);
         if (set_name) {
            assert (sd->set_name);
            assert (strcmp(sd->set_name, set_name) == 0);
         }
      } else if (strcmp("type", bson_iter_key (&server_iter)) == 0) {
         assert (sd->type == server_type_from_test(bson_iter_utf8(&server_iter, NULL)));
      } else {
         printf ("ERROR: unparsed field %s\n", bson_iter_key(&server_iter));
         assert (0);
      }
   }
}

/*
 *-----------------------------------------------------------------------
 *
 * Run the JSON tests from the Server Discovery and Monitoring spec.
 *
 *-----------------------------------------------------------------------
 */
static void
test_sdam_cb (bson_t *test)
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
   uint32_t len;
   const uint8_t *iter_data;
   const char *set_name;
   const char *hostname;

   /* parse out the uri and use it to create a client */
   assert (bson_iter_init_find(&iter, test, "uri"));
   client = mongoc_client_new(bson_iter_utf8(&iter, NULL));

   /* for each phase, parse and validate */
   assert (bson_iter_init_find(&iter, test, "phases"));
   bson_iter_array (&iter, &len, &iter_data);
   assert (bson_init_static (&phases, iter_data, len));
   bson_iter_init (&phase_iter, &phases);

   while (bson_iter_next (&phase_iter)) {
      bson_iter_document (&phase_iter, &len, &iter_data);
      bson_init_static (&phase, iter_data, len);

      /* grab ismaster responses out and feed them to topology */
      assert (bson_iter_init_find(&phase_field_iter, &phase, "responses"));
      bson_iter_array (&phase_field_iter, &len, &iter_data);
      assert (bson_init_static (&ismasters, iter_data, len));
      bson_iter_init (&ismaster_iter, &ismasters);

      while (bson_iter_next (&ismaster_iter)) {
         bson_iter_array (&ismaster_iter, &len, &iter_data);
         bson_init_static (&ismaster, iter_data, len);

         /* fetch server description for this server based on its hostname */
         bson_iter_init_find (&ismaster_field_iter, &ismaster, "0");
         sd = _server_description_by_hostname(&client->topology->description,
                                              bson_iter_utf8(&ismaster_field_iter, NULL));

         /* if server has been removed from topology, skip */
         /* TODO: ASSURE that the manager has the same behavior */
         if (!sd) continue;

         bson_iter_init_find (&ismaster_field_iter, &ismaster, "1");
         bson_iter_document (&ismaster_field_iter, &len, &iter_data);
         bson_init_static (&response, iter_data, len);

         /* send ismaster through the topology description's handler */
         mongoc_topology_description_handle_ismaster(&client->topology->description,
                                                     client->topology->scanner,
                                                     sd,
                                                     &response,
                                                     15,
                                                     &error);
      }

      /* parse out "outcome" and validate */
      assert (bson_iter_init_find(&phase_field_iter, &phase, "outcome"));
      bson_iter_document (&phase_field_iter, &len, &iter_data);
      bson_init_static (&outcome, iter_data, len);
      bson_iter_init (&outcome_iter, &outcome);

      while (bson_iter_next (&outcome_iter)) {
         if (strcmp ("servers", bson_iter_key (&outcome_iter)) == 0) {
            bson_iter_document (&outcome_iter, &len, &iter_data);
            bson_init_static (&servers, iter_data, len);
            ASSERT_CMPINT (
               bson_count_keys (&servers), ==,
               (int) client->topology->description.servers->items_len);

            bson_iter_init (&servers_iter, &servers);

            /* for each server, ensure topology has a matching entry */
            while (bson_iter_next (&servers_iter)) {
               hostname = bson_iter_key (&servers_iter);
               bson_iter_document(&servers_iter, &len, &iter_data);
               bson_init_static (&server, iter_data, len);

               _topology_has_description(&client->topology->description,
                                         &server,
                                         hostname);
            }

         } else if (strcmp ("setName", bson_iter_key (&outcome_iter)) == 0) {
            set_name = bson_iter_utf8(&outcome_iter, NULL);
            if (set_name) {
               assert (&client->topology->description.set_name);
               assert (strcmp(client->topology->description.set_name, set_name) == 0);
            }
         } else if (strcmp ("topologyType", bson_iter_key (&outcome_iter)) == 0) {
            assert (strcmp(topology_type_to_string(client->topology->description.type),
                           bson_iter_utf8(&outcome_iter, NULL)) == 0);
         } else {
            printf ("ERROR: unparsed test field %s\n", bson_iter_key (&outcome_iter));
            assert (false);
         }
      }
   }
   mongoc_client_destroy (client);
}

/*
 *-----------------------------------------------------------------------
 *
 * Runner for the JSON tests for server discovery and monitoring..
 *
 *-----------------------------------------------------------------------
 */
static void
test_all_spec_tests (TestSuite *suite)
{
   /* Single */
   install_json_test_suite(suite, "tests/json/server_discovery_and_monitoring/single",
                       &test_sdam_cb);

   /* Replica set */
   install_json_test_suite(suite, "tests/json/server_discovery_and_monitoring/rs",
                       &test_sdam_cb);

   /* Sharded */
   install_json_test_suite(suite, "tests/json/server_discovery_and_monitoring/sharded",
                       &test_sdam_cb);
}

void
test_sdam_install (TestSuite *suite)
{
   test_all_spec_tests(suite);
}
