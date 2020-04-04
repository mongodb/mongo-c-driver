#include <mongoc/mongoc.h>

#include <mongoc/mongoc-set-private.h>

#include "json-test.h"

#include "mongoc/mongoc-client-private.h"
#include "test-libmongoc.h"

#ifdef BSON_HAVE_STRINGS_H
#include <strings.h>
#endif


static void
_topology_has_description (mongoc_topology_description_t *topology,
                           bson_t *server,
                           const char *address)
{
   mongoc_server_description_t *sd;
   bson_iter_t server_iter;
   const char *server_type;
   const char *set_name;

   sd = server_description_by_hostname (topology, address);
   BSON_ASSERT (sd);

   bson_iter_init (&server_iter, server);
   while (bson_iter_next (&server_iter)) {
      if (strcmp ("setName", bson_iter_key (&server_iter)) == 0) {
         set_name = bson_iter_utf8 (&server_iter, NULL);
         if (set_name) {
            BSON_ASSERT (sd->set_name);
            ASSERT_CMPSTR (sd->set_name, set_name);
         }
      } else if (strcmp ("type", bson_iter_key (&server_iter)) == 0) {
         server_type = bson_iter_utf8 (&server_iter, NULL);
         if (sd->type != server_type_from_test (server_type)) {
            fprintf (stderr,
                     "expected server type %s not %s\n",
                     server_type,
                     mongoc_server_description_type (sd));
            abort ();
         }
      } else if (strcmp ("setVersion", bson_iter_key (&server_iter)) == 0) {
         int64_t expected_set_version;
         if (BSON_ITER_HOLDS_NULL (&server_iter)) {
            expected_set_version = MONGOC_NO_SET_VERSION;
         } else {
            expected_set_version = bson_iter_as_int64 (&server_iter);
         }
         BSON_ASSERT (sd->set_version == expected_set_version);
      } else if (strcmp ("electionId", bson_iter_key (&server_iter)) == 0) {
         bson_oid_t expected_oid;
         if (BSON_ITER_HOLDS_NULL (&server_iter)) {
            bson_oid_init_from_string (&expected_oid,
                                       "000000000000000000000000");
         } else {
            ASSERT (BSON_ITER_HOLDS_OID (&server_iter));
            bson_oid_copy (bson_iter_oid (&server_iter), &expected_oid);
         }

         ASSERT_CMPOID (&sd->election_id, &expected_oid);
      } else {
         fprintf (
            stderr, "ERROR: unparsed field %s\n", bson_iter_key (&server_iter));
         BSON_ASSERT (0);
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
   mongoc_client_t *client;
   mongoc_topology_description_t *td;
   bson_t phase;
   bson_t phases;
   bson_t servers;
   bson_t server;
   bson_t outcome;
   bson_iter_t phase_iter;
   bson_iter_t phase_field_iter;
   bson_iter_t servers_iter;
   bson_iter_t outcome_iter;
   bson_iter_t iter;
   const char *set_name;
   const char *hostname;

   /* parse out the uri and use it to create a client */
   BSON_ASSERT (bson_iter_init_find (&iter, test, "uri"));
   client = mongoc_client_new (bson_iter_utf8 (&iter, NULL));
   td = &client->topology->description;

   /* for each phase, parse and validate */
   BSON_ASSERT (bson_iter_init_find (&iter, test, "phases"));
   bson_iter_bson (&iter, &phases);
   bson_iter_init (&phase_iter, &phases);

   while (bson_iter_next (&phase_iter)) {
      bson_iter_bson (&phase_iter, &phase);

      process_sdam_test_ismaster_responses (&phase, td);

      /* parse out "outcome" and validate */
      BSON_ASSERT (bson_iter_init_find (&phase_field_iter, &phase, "outcome"));
      bson_iter_bson (&phase_field_iter, &outcome);
      bson_iter_init (&outcome_iter, &outcome);

      while (bson_iter_next (&outcome_iter)) {
         if (strcmp ("servers", bson_iter_key (&outcome_iter)) == 0) {
            bson_iter_bson (&outcome_iter, &servers);
            ASSERT_CMPINT (
               bson_count_keys (&servers), ==, (int) td->servers->items_len);

            bson_iter_init (&servers_iter, &servers);

            /* for each server, ensure topology has a matching entry */
            while (bson_iter_next (&servers_iter)) {
               hostname = bson_iter_key (&servers_iter);
               bson_iter_bson (&servers_iter, &server);

               _topology_has_description (td, &server, hostname);
            }

         } else if (strcmp ("setName", bson_iter_key (&outcome_iter)) == 0) {
            set_name = bson_iter_utf8 (&outcome_iter, NULL);
            if (set_name) {
               BSON_ASSERT (td->set_name);
               ASSERT_CMPSTR (td->set_name, set_name);
            }
         } else if (strcmp ("topologyType", bson_iter_key (&outcome_iter)) ==
                    0) {
            ASSERT_CMPSTR (mongoc_topology_description_type (td),
                           bson_iter_utf8 (&outcome_iter, NULL));
         } else if (strcmp ("logicalSessionTimeoutMinutes",
                            bson_iter_key (&outcome_iter)) == 0) {
            if (BSON_ITER_HOLDS_NULL (&outcome_iter)) {
               ASSERT_CMPINT64 (td->session_timeout_minutes,
                                ==,
                                (int64_t) MONGOC_NO_SESSIONS);
            } else {
               ASSERT_CMPINT64 (td->session_timeout_minutes,
                                ==,
                                bson_iter_as_int64 (&outcome_iter));
            }
         } else if (strcmp ("compatible", bson_iter_key (&outcome_iter)) == 0) {
            if (bson_iter_as_bool (&outcome_iter)) {
               ASSERT_CMPINT (0, ==, td->compatibility_error.domain);
            } else {
               ASSERT_ERROR_CONTAINS (td->compatibility_error,
                                      MONGOC_ERROR_PROTOCOL,
                                      MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                                      "");
            }
         } else if (strcmp ("maxSetVersion", bson_iter_key (&outcome_iter)) ==
                    0) {
            ASSERT_CMPINT64 (
               bson_iter_as_int64 (&outcome_iter), ==, td->max_set_version);
         } else if (strcmp ("maxElectionId", bson_iter_key (&outcome_iter)) ==
                    0) {
            const bson_oid_t *expected_oid;
            expected_oid = bson_iter_oid (&outcome_iter);

            if (!bson_oid_equal (expected_oid, &td->max_election_id)) {
               char expected_oid_str[25];
               char actual_oid_str[25];

               bson_oid_to_string (expected_oid, expected_oid_str);
               bson_oid_to_string (&td->max_election_id, actual_oid_str);
               test_error ("ERROR: Expected topology description's "
                           "maxElectionId to be %s, but was %s",
                           expected_oid_str,
                           actual_oid_str);
            }
         } else {
            fprintf (stderr,
                     "ERROR: unparsed test field %s\n",
                     bson_iter_key (&outcome_iter));
            BSON_ASSERT (false);
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
   char resolved[PATH_MAX];

   /* Single */
   ASSERT (
      realpath (JSON_DIR "/server_discovery_and_monitoring/single", resolved));
   install_json_test_suite (suite, resolved, &test_sdam_cb);

   /* Replica set */
   test_framework_resolve_path (JSON_DIR "/server_discovery_and_monitoring/rs",
                                resolved);
   install_json_test_suite (suite, resolved, &test_sdam_cb);

   /* Sharded */
   ASSERT (
      realpath (JSON_DIR "/server_discovery_and_monitoring/sharded", resolved));
   install_json_test_suite (suite, resolved, &test_sdam_cb);

   /* Tests not in official Server Discovery And Monitoring Spec */
   ASSERT (realpath (JSON_DIR "/server_discovery_and_monitoring/supplemental",
                     resolved));
   install_json_test_suite (suite, resolved, &test_sdam_cb);
}

static void
test_topology_discovery (void *ctx)
{
   char *host_and_port;
   char *replset_name;
   char *uri_str;
   char *uri_str_auth;
   mongoc_client_t *client;
   mongoc_read_prefs_t *prefs;
   mongoc_server_description_t *sd_secondary;
   mongoc_host_list_t *hl_secondary;
   mongoc_collection_t *collection;
   bson_t doc = BSON_INITIALIZER;
   bson_t reply;
   bson_error_t error;
   bool r;

   host_and_port = test_framework_get_host_and_port ();
   replset_name = test_framework_replset_name ();
   uri_str = test_framework_get_uri_str ();

   client = mongoc_client_new (uri_str);
   test_framework_set_ssl_opts (client);
   prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   sd_secondary = mongoc_client_select_server (client,
                                     false, /* for reads */
                                     prefs,
                                     &error);
   ASSERT_OR_PRINT (sd_secondary, error);
   hl_secondary = mongoc_server_description_host (sd_secondary);

   /* Scenario: given a replica set deployment with a secondary, where HOST is
    * the address of the secondary, create a MongoClient using
    * ``mongodb://HOST/?directConnection=false`` as the URI.
    * Attempt a write to a collection.
    *
    * Outcome: Verify that the write succeeded. */
   bson_free (uri_str);
   uri_str = bson_strdup_printf (
      "mongodb://%s/?directConnection=false", hl_secondary->host_and_port);
   uri_str_auth = test_framework_add_user_password_from_env (uri_str);

   mongoc_client_destroy (client);
   client = mongoc_client_new (uri_str_auth);
   test_framework_set_ssl_opts (client);
   collection = get_test_collection (client, "sdam_dc_test");
   BSON_APPEND_UTF8 (&doc, "hello", "world");
   r = mongoc_collection_insert_one (collection, &doc, NULL, &reply, &error);
   ASSERT_OR_PRINT (r, error);
   ASSERT_CMPINT32 (bson_lookup_int32 (&reply, "insertedCount"), ==, 1);

   bson_destroy (&reply);
   bson_destroy (&doc);
   mongoc_server_description_destroy (sd_secondary);
   mongoc_read_prefs_destroy (prefs);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_free (uri_str_auth);
   bson_free (uri_str);
   bson_free (replset_name);
   bson_free (host_and_port);

}

static void
test_direct_connection (void *ctx)
{
   char *host_and_port;
   char *replset_name;
   char *uri_str;
   char *uri_str_auth;
   mongoc_client_t *client;
   mongoc_read_prefs_t *prefs;
   mongoc_server_description_t *sd_secondary;
   mongoc_host_list_t *hl_secondary;
   mongoc_collection_t *collection;
   bson_t doc = BSON_INITIALIZER;
   bson_t reply;
   bson_error_t error;
   bool r;

   host_and_port = test_framework_get_host_and_port ();
   replset_name = test_framework_replset_name ();
   uri_str = test_framework_get_uri_str ();

   client = mongoc_client_new (uri_str);
   test_framework_set_ssl_opts (client);
   mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);
   prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   sd_secondary = mongoc_client_select_server (client,
                                     false, /* for reads */
                                     prefs,
                                     &error);
   ASSERT_OR_PRINT (sd_secondary, error);
   hl_secondary = mongoc_server_description_host (sd_secondary);

   /* Scenario: given a replica set deployment with a secondary, where HOST is
    * the address of the secondary, create a MongoClient using
    * ``mongodb://HOST/?directConnection=true`` as the URI.
    * Attempt a write to a collection.
    *
    * Outcome: Verify that the write failed with a NotMaster error. */
   bson_free (uri_str);
   uri_str = bson_strdup_printf (
      "mongodb://%s/?directConnection=true", hl_secondary->host_and_port);
   uri_str_auth = test_framework_add_user_password_from_env (uri_str);

   mongoc_client_destroy (client);
   client = mongoc_client_new (uri_str_auth);
   test_framework_set_ssl_opts (client);
   collection = get_test_collection (client, "sdam_dc_test");
   BSON_APPEND_UTF8 (&doc, "hello", "world");
   r = mongoc_collection_insert_one (collection, &doc, NULL, &reply, &error);
   ASSERT_OR_PRINT (!r, error);
   ASSERT (strstr (error.message, "not master"));

   bson_destroy (&reply);
   bson_destroy (&doc);
   mongoc_server_description_destroy (sd_secondary);
   mongoc_read_prefs_destroy (prefs);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_free (uri_str_auth);
   bson_free (uri_str);
   bson_free (replset_name);
   bson_free (host_and_port);

}

static void
test_existing_behavior (void *ctx)
{
   char *host_and_port;
   char *replset_name;
   char *uri_str;
   char *uri_str_auth;
   mongoc_client_t *client;
   mongoc_read_prefs_t *prefs;
   mongoc_server_description_t *sd_secondary;
   mongoc_host_list_t *hl_secondary;
   mongoc_collection_t *collection;
   bson_t doc = BSON_INITIALIZER;
   bson_t reply;
   bson_error_t error;
   bool r;

   host_and_port = test_framework_get_host_and_port ();
   replset_name = test_framework_replset_name ();
   uri_str = test_framework_get_uri_str ();

   client = mongoc_client_new (uri_str);
   test_framework_set_ssl_opts (client);
   mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);
   prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   sd_secondary = mongoc_client_select_server (client,
                                     false, /* for reads */
                                     prefs,
                                     &error);
   ASSERT_OR_PRINT (sd_secondary, error);
   hl_secondary = mongoc_server_description_host (sd_secondary);

   /* Scenario: given a replica set deployment with a secondary, where HOST is
    * the address of the secondary, create a MongoClient using
    * ``mongodb://HOST/`` as the URI.
    * Attempt a write to a collection.
    *
    * Outcome: Verify that the write succeeded or failed depending on existing
    * driver behavior with respect to the starting topology. */
   bson_free (uri_str);
   uri_str = bson_strdup_printf (
      "mongodb://%s/", hl_secondary->host_and_port);
   uri_str_auth = test_framework_add_user_password_from_env (uri_str);

   mongoc_client_destroy (client);
   client = mongoc_client_new (uri_str_auth);
   test_framework_set_ssl_opts (client);
   collection = get_test_collection (client, "sdam_dc_test");
   BSON_APPEND_UTF8 (&doc, "hello", "world");
   r = mongoc_collection_insert_one (collection, &doc, NULL, &reply, &error);
   ASSERT_OR_PRINT (!r, error);
   ASSERT (strstr (error.message, "not master"));

   bson_destroy (&reply);
   bson_destroy (&doc);
   mongoc_server_description_destroy (sd_secondary);
   mongoc_read_prefs_destroy (prefs);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_free (uri_str_auth);
   bson_free (uri_str);
   bson_free (replset_name);
   bson_free (host_and_port);

}

void
test_sdam_install (TestSuite *suite)
{
   test_all_spec_tests (suite);
   TestSuite_AddFull (
      suite,
      "/server_discovery_and_monitoring/topology/discovery",
      test_topology_discovery,
      NULL /* dtor */,
      NULL /* ctx */,
      test_framework_skip_if_not_replset);
   TestSuite_AddFull (
      suite,
      "/server_discovery_and_monitoring/directconnection",
      test_direct_connection,
      NULL /* dtor */,
      NULL /* ctx */,
      test_framework_skip_if_not_replset);
   TestSuite_AddFull (
      suite,
      "/server_discovery_and_monitoring/existing/behavior",
      test_existing_behavior,
      NULL /* dtor */,
      NULL /* ctx */,
      test_framework_skip_if_not_replset);
}
