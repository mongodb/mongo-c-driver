/*
 * Copyright 2015 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "mongoc-config.h"
#include "mongoc-collection-private.h"
#include "mongoc-server-description-private.h"
#include "mongoc-topology-description-private.h"
#include "mongoc-topology-private.h"
#include "mongoc-util-private.h"
#include "mongoc-util-private.h"

#include "TestSuite.h"
#include "test-conveniences.h"

#include "json-test.h"
#include "test-libmongoc.h"

#ifdef _MSC_VER
#include <io.h>
#else
#include <dirent.h>
#endif

#ifdef BSON_HAVE_STRINGS_H
#include <strings.h>
#endif


mongoc_topology_description_type_t
topology_type_from_test (const char *type)
{
   if (strcmp (type, "ReplicaSetWithPrimary") == 0) {
      return MONGOC_TOPOLOGY_RS_WITH_PRIMARY;
   } else if (strcmp (type, "ReplicaSetNoPrimary") == 0) {
      return MONGOC_TOPOLOGY_RS_NO_PRIMARY;
   } else if (strcmp (type, "Unknown") == 0) {
      return MONGOC_TOPOLOGY_UNKNOWN;
   } else if (strcmp (type, "Single") == 0) {
      return MONGOC_TOPOLOGY_SINGLE;
   } else if (strcmp (type, "Sharded") == 0) {
      return MONGOC_TOPOLOGY_SHARDED;
   }

   fprintf (stderr, "can't parse this: %s", type);
   BSON_ASSERT (0);
   return 0;
}

mongoc_server_description_type_t
server_type_from_test (const char *type)
{
   if (strcmp (type, "RSPrimary") == 0) {
      return MONGOC_SERVER_RS_PRIMARY;
   } else if (strcmp (type, "RSSecondary") == 0) {
      return MONGOC_SERVER_RS_SECONDARY;
   } else if (strcmp (type, "Standalone") == 0) {
      return MONGOC_SERVER_STANDALONE;
   } else if (strcmp (type, "Mongos") == 0) {
      return MONGOC_SERVER_MONGOS;
   } else if (strcmp (type, "PossiblePrimary") == 0) {
      return MONGOC_SERVER_POSSIBLE_PRIMARY;
   } else if (strcmp (type, "RSArbiter") == 0) {
      return MONGOC_SERVER_RS_ARBITER;
   } else if (strcmp (type, "RSOther") == 0) {
      return MONGOC_SERVER_RS_OTHER;
   } else if (strcmp (type, "RSGhost") == 0) {
      return MONGOC_SERVER_RS_GHOST;
   } else if (strcmp (type, "Unknown") == 0) {
      return MONGOC_SERVER_UNKNOWN;
   }
   fprintf (stderr, "ERROR: Unknown server type %s\n", type);
   BSON_ASSERT (0);
   return 0;
}


static mongoc_read_mode_t
read_mode_from_test (const char *mode)
{
   if (strcmp (mode, "Primary") == 0) {
      return MONGOC_READ_PRIMARY;
   } else if (strcmp (mode, "PrimaryPreferred") == 0) {
      return MONGOC_READ_PRIMARY_PREFERRED;
   } else if (strcmp (mode, "Secondary") == 0) {
      return MONGOC_READ_SECONDARY;
   } else if (strcmp (mode, "SecondaryPreferred") == 0) {
      return MONGOC_READ_SECONDARY_PREFERRED;
   } else if (strcmp (mode, "Nearest") == 0) {
      return MONGOC_READ_NEAREST;
   }

   return MONGOC_READ_PRIMARY;
}


static mongoc_ss_optype_t
optype_from_test (const char *op)
{
   if (strcmp (op, "read") == 0) {
      return MONGOC_SS_READ;
   } else if (strcmp (op, "write") == 0) {
      return MONGOC_SS_WRITE;
   }

   return MONGOC_SS_READ;
}


/*
 *-----------------------------------------------------------------------
 *
 * server_description_by_hostname --
 *
 *      Return a reference to a mongoc_server_description_t or NULL.
 *
 *-----------------------------------------------------------------------
 */
mongoc_server_description_t *
server_description_by_hostname (mongoc_topology_description_t *topology,
                                const char *address)
{
   mongoc_set_t *set = topology->servers;
   mongoc_server_description_t *server_iter;
   int i;

   for (i = 0; i < set->items_len; i++) {
      server_iter = (mongoc_server_description_t *) mongoc_set_get_item (
         topology->servers, i);

      if (strcasecmp (address, server_iter->connection_address) == 0) {
         return server_iter;
      }
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------
 *
 * process_sdam_test_ismaster_responses --
 *
 *      Update a topology description with the ismaster responses in a "phase"
 *      from an SDAM or SDAM Monitoring test, like:
 *
 *      [
 *          [
 *              "a:27017",
 *              {
 *                  "ok": 1,
 *                  "ismaster": false
 *              }
 *          ]
 *      ]
 *
 * See:
 * https://github.com/mongodb/specifications/tree/master/source/server-discovery-and-monitoring/tests
 *
 *-----------------------------------------------------------------------
 */
void
process_sdam_test_ismaster_responses (bson_t *phase,
                                      mongoc_topology_description_t *td)
{
   mongoc_server_description_t *sd;
   bson_t ismasters;
   bson_t ismaster;
   bson_t response;
   bson_iter_t phase_field_iter;
   bson_iter_t ismaster_iter;
   bson_iter_t ismaster_field_iter;
   const char *hostname;

   /* grab ismaster responses out and feed them to topology */
   BSON_ASSERT (bson_iter_init_find (&phase_field_iter, phase, "responses"));
   bson_iter_bson (&phase_field_iter, &ismasters);
   bson_iter_init (&ismaster_iter, &ismasters);

   while (bson_iter_next (&ismaster_iter)) {
      bson_iter_bson (&ismaster_iter, &ismaster);

      bson_iter_init_find (&ismaster_field_iter, &ismaster, "0");
      hostname = bson_iter_utf8 (&ismaster_field_iter, NULL);
      sd = server_description_by_hostname (td, hostname);

      /* if server has been removed from topology, skip */
      if (!sd) {
         continue;
      }

      bson_iter_init_find (&ismaster_field_iter, &ismaster, "1");
      bson_iter_bson (&ismaster_field_iter, &response);

      /* send ismaster through the topology description's handler */
      mongoc_topology_description_handle_ismaster (
         td, sd->id, &response, 1, NULL);
   }
}


/*
 *-----------------------------------------------------------------------
 *
 * check_json_apm_events --
 *
 *      Compare actual APM events with expected sequence. The two docs
 *      are like:
 *
 * [
 *   {
 *     "command_started_event": {
 *       "command": { ... },
 *       "command_name": "count",
 *       "database_name": "command-monitoring-tests"
 *     }
 *   },
 *   {
 *     "command_failed_event": {
 *       "command_name": "count"
 *     }
 *   }
 * ]
 *
 *-----------------------------------------------------------------------
 */
void
check_json_apm_events (const bson_t *events, const bson_t *expectations)
{
   char errmsg[1000] = {0};
   match_ctx_t ctx = {0};
   uint32_t expected_keys;
   uint32_t actual_keys;

   /* Old mongod returns a double for "count", newer returns int32.
    * Ignore this and other insignificant type differences. */
   ctx.strict_numeric_types = false;
   ctx.errmsg = errmsg;
   ctx.errmsg_len = sizeof errmsg;

   expected_keys = bson_count_keys (expectations);
   actual_keys = bson_count_keys (events);

   if (expected_keys != actual_keys) {
      test_error ("command monitoring test failed expectations:\n\n"
                  "%s\n\n"
                  "events:\n%s\n\n"
                  "expected %" PRIu32 " events, got %" PRIu32,
                  bson_as_canonical_extended_json (expectations, NULL),
                  bson_as_canonical_extended_json (events, NULL),
                  expected_keys,
                  actual_keys);

      abort ();
   }

   if (!match_bson_with_ctx (events, expectations, false, &ctx)) {
      test_error ("command monitoring test failed expectations:\n\n"
                  "%s\n\n"
                  "events:\n%s\n\n%s",
                  bson_as_canonical_extended_json (expectations, NULL),
                  bson_as_canonical_extended_json (events, NULL),
                  errmsg);

      abort ();
   }
}


/*
 *-----------------------------------------------------------------------
 *
 * test_server_selection_logic_cb --
 *
 *      Runs the JSON tests for server selection logic that are
 *      included with the Server Selection spec.
 *
 *-----------------------------------------------------------------------
 */
void
test_server_selection_logic_cb (bson_t *test)
{
   bool expected_error;
   bson_error_t error;
   int32_t heartbeat_msec;
   mongoc_topology_description_t topology;
   mongoc_server_description_t *sd;
   mongoc_read_prefs_t *read_prefs;
   mongoc_read_mode_t read_mode;
   mongoc_ss_optype_t op;
   bson_iter_t iter;
   bson_iter_t topology_iter;
   bson_iter_t server_iter;
   bson_iter_t sd_iter;
   bson_iter_t read_pref_iter;
   bson_iter_t tag_sets_iter;
   bson_iter_t last_write_iter;
   bson_iter_t expected_servers_iter;
   bson_t first_tag_set;
   bson_t test_topology;
   bson_t test_servers;
   bson_t server;
   bson_t test_read_pref;
   bson_t test_tag_sets;
   uint32_t i = 0;
   bool matched_servers[50];
   mongoc_array_t selected_servers;

   _mongoc_array_init (&selected_servers,
                       sizeof (mongoc_server_description_t *));

   BSON_ASSERT (test);

   expected_error =
      bson_iter_init_find (&iter, test, "error") && bson_iter_as_bool (&iter);

   heartbeat_msec = MONGOC_TOPOLOGY_HEARTBEAT_FREQUENCY_MS_SINGLE_THREADED;

   if (bson_iter_init_find (&iter, test, "heartbeatFrequencyMS")) {
      heartbeat_msec = bson_iter_int32 (&iter);
   }

   /* pull out topology description field */
   BSON_ASSERT (bson_iter_init_find (&iter, test, "topology_description"));
   bson_iter_bson (&iter, &test_topology);

   /* set topology state from test */
   BSON_ASSERT (bson_iter_init_find (&topology_iter, &test_topology, "type"));

   mongoc_topology_description_init (&topology, heartbeat_msec);
   topology.type =
      topology_type_from_test (bson_iter_utf8 (&topology_iter, NULL));

   /* for each server description in test, add server to our topology */
   BSON_ASSERT (
      bson_iter_init_find (&topology_iter, &test_topology, "servers"));
   bson_iter_bson (&topology_iter, &test_servers);

   bson_iter_init (&server_iter, &test_servers);
   while (bson_iter_next (&server_iter)) {
      bson_iter_bson (&server_iter, &server);

      /* initialize new server description with given address */
      sd = (mongoc_server_description_t *) bson_malloc0 (sizeof *sd);
      BSON_ASSERT (bson_iter_init_find (&sd_iter, &server, "address"));
      mongoc_server_description_init (sd, bson_iter_utf8 (&sd_iter, NULL), i++);

      BSON_ASSERT (bson_iter_init_find (&sd_iter, &server, "type"));
      sd->type = server_type_from_test (bson_iter_utf8 (&sd_iter, NULL));

      if (bson_iter_init_find (&sd_iter, &server, "avg_rtt_ms")) {
         sd->round_trip_time_msec = bson_iter_int32 (&sd_iter);
      } else if (sd->type != MONGOC_SERVER_UNKNOWN) {
         test_error ("%s has no avg_rtt_ms", sd->host.host_and_port);
         abort ();
      }

      if (bson_iter_init_find (&sd_iter, &server, "maxWireVersion")) {
         sd->max_wire_version = (int32_t) bson_iter_as_int64 (&sd_iter);
      }

      if (bson_iter_init_find (&sd_iter, &server, "lastUpdateTime")) {
         sd->last_update_time_usec = bson_iter_as_int64 (&sd_iter) * 1000;
      }

      if (bson_iter_init_find (&sd_iter, &server, "lastWrite")) {
         BSON_ASSERT (BSON_ITER_HOLDS_DOCUMENT (&sd_iter) &&
                      bson_iter_recurse (&sd_iter, &last_write_iter) &&
                      bson_iter_find (&last_write_iter, "lastWriteDate") &&
                      BSON_ITER_HOLDS_INT (&last_write_iter));
         sd->last_write_date_ms = bson_iter_as_int64 (&last_write_iter);
      }

      if (bson_iter_init_find (&sd_iter, &server, "tags")) {
         bson_destroy (&sd->tags);
         bson_iter_bson (&sd_iter, &sd->tags);
      }

      /* add new server to our topology description */
      mongoc_set_add (topology.servers, sd->id, sd);
   }

   /* create read preference document from test */
   BSON_ASSERT (bson_iter_init_find (&iter, test, "read_preference"));
   bson_iter_bson (&iter, &test_read_pref);

   if (bson_iter_init_find (&read_pref_iter, &test_read_pref, "mode")) {
      read_mode = read_mode_from_test (bson_iter_utf8 (&read_pref_iter, NULL));
      ASSERT_CMPINT (read_mode, !=, 0);
   } else {
      read_mode = MONGOC_READ_PRIMARY;
   }

   read_prefs = mongoc_read_prefs_new (read_mode);

   if (bson_iter_init_find (&read_pref_iter, &test_read_pref, "tag_sets")) {
      /* ignore  "tag_sets: [{}]" */
      if (bson_iter_recurse (&read_pref_iter, &tag_sets_iter) &&
          bson_iter_next (&tag_sets_iter) &&
          BSON_ITER_HOLDS_DOCUMENT (&tag_sets_iter)) {
         bson_iter_bson (&tag_sets_iter, &first_tag_set);
         if (!bson_empty (&first_tag_set)) {
            /* not empty */
            bson_iter_bson (&read_pref_iter, &test_tag_sets);
            mongoc_read_prefs_set_tags (read_prefs, &test_tag_sets);
         }
      }
   }

   if (bson_iter_init_find (
          &read_pref_iter, &test_read_pref, "maxStalenessSeconds")) {
      mongoc_read_prefs_set_max_staleness_seconds (
         read_prefs, bson_iter_as_int64 (&read_pref_iter));
   }

   /* get operation type */
   op = MONGOC_SS_READ;

   if (bson_iter_init_find (&iter, test, "operation")) {
      op = optype_from_test (bson_iter_utf8 (&iter, NULL));
   }

   if (expected_error) {
      BSON_ASSERT (!mongoc_read_prefs_is_valid (read_prefs) ||
                   !mongoc_topology_compatible (&topology, read_prefs, &error));
      goto DONE;
   }

   /* no expected error */
   BSON_ASSERT (mongoc_read_prefs_is_valid (read_prefs));
   BSON_ASSERT (mongoc_topology_compatible (&topology, read_prefs, &error));

   /* read in latency window servers */
   BSON_ASSERT (bson_iter_init_find (&iter, test, "in_latency_window"));

   /* TODO: use topology_select instead? */
   mongoc_topology_description_suitable_servers (
      &selected_servers,
      op,
      &topology,
      read_prefs,
      MONGOC_TOPOLOGY_LOCAL_THRESHOLD_MS);

   /* check each server in expected_servers is in selected_servers */
   memset (matched_servers, 0, sizeof (matched_servers));
   bson_iter_recurse (&iter, &expected_servers_iter);
   while (bson_iter_next (&expected_servers_iter)) {
      bool found = false;
      bson_iter_t host;

      BSON_ASSERT (bson_iter_recurse (&expected_servers_iter, &host));
      BSON_ASSERT (bson_iter_find (&host, "address"));

      for (i = 0; i < selected_servers.len; i++) {
         sd = _mongoc_array_index (
            &selected_servers, mongoc_server_description_t *, i);

         if (strcmp (sd->host.host_and_port, bson_iter_utf8 (&host, NULL)) ==
             0) {
            found = true;
            break;
         }
      }

      if (!found) {
         test_error ("Should have been selected but wasn't: %s",
                     bson_iter_utf8 (&host, NULL));
         abort ();
      }

      matched_servers[i] = true;
   }

   /* check each server in selected_servers is in expected_servers */
   for (i = 0; i < selected_servers.len; i++) {
      if (!matched_servers[i]) {
         sd = _mongoc_array_index (
            &selected_servers, mongoc_server_description_t *, i);

         test_error ("Shouldn't have been selected but was: %s",
                     sd->host.host_and_port);
         abort ();
      }
   }

DONE:
   mongoc_read_prefs_destroy (read_prefs);
   mongoc_topology_description_destroy (&topology);
   _mongoc_array_destroy (&selected_servers);
}

/*
 *-----------------------------------------------------------------------
 *
 * assemble_path --
 *
 *       Given a parent directory and filename, compile a full path to
 *       the child file.
 *
 *       "dst" receives the joined path, delimited by "/" even on Windows.
 *
 *-----------------------------------------------------------------------
 */
void
assemble_path (const char *parent_path,
               const char *child_name,
               char *dst /* OUT */)
{
   char *p;
   int path_len = (int) strlen (parent_path);
   int name_len = (int) strlen (child_name);

   BSON_ASSERT (path_len + name_len + 1 < MAX_TEST_NAME_LENGTH);

   memset (dst, '\0', MAX_TEST_NAME_LENGTH * sizeof (char));
   strncat (dst, parent_path, path_len);
   strncat (dst, "/", 1);
   strncat (dst, child_name, name_len);

   for (p = dst; *p; ++p) {
      if (*p == '\\') {
         *p = '/';
      }
   }
}

/*
 *-----------------------------------------------------------------------
 *
 * collect_tests_from_dir --
 *
 *       Recursively search the directory at @dir_path for files with
 *       '.json' in their filenames. Append all found file paths to
 *       @paths, and return the number of files found.
 *
 *-----------------------------------------------------------------------
 */
int
collect_tests_from_dir (char (*paths)[MAX_TEST_NAME_LENGTH] /* OUT */,
                        const char *dir_path,
                        int paths_index,
                        int max_paths)
{
#ifdef _MSC_VER
   char *dir_path_plus_star;
   intptr_t handle;
   struct _finddata_t info;

   char child_path[MAX_TEST_NAME_LENGTH];

   dir_path_plus_star = bson_strdup_printf ("%s/*", dir_path);
   handle = _findfirst (dir_path_plus_star, &info);

   if (handle == -1) {
      bson_free (dir_path_plus_star);
      return 0;
   }

   while (1) {
      BSON_ASSERT (paths_index < max_paths);

      if (info.attrib & _A_SUBDIR) {
         /* recursively call on child directories */
         if (strcmp (info.name, "..") != 0 && strcmp (info.name, ".") != 0) {
            assemble_path (dir_path, info.name, child_path);
            paths_index = collect_tests_from_dir (
               paths, child_path, paths_index, max_paths);
         }
      } else if (strstr (info.name, ".json")) {
         /* if this is a JSON test, collect its path */
         assemble_path (dir_path, info.name, paths[paths_index++]);
      }

      if (_findnext (handle, &info) == -1) {
         break;
      }
   }

   bson_free (dir_path_plus_star);
   _findclose (handle);

   return paths_index;
#else
   struct dirent *entry;
   struct stat dir_stat;
   char child_path[MAX_TEST_NAME_LENGTH];
   DIR *dir;

   dir = opendir (dir_path);
   if (!dir) {
      MONGOC_ERROR ("Cannot open \"%s\"", dir_path);
      abort ();
   }

   while ((entry = readdir (dir))) {
      BSON_ASSERT (paths_index < max_paths);
      if (strcmp (entry->d_name, "..") == 0 ||
          strcmp (entry->d_name, ".") == 0) {
         continue;
      }

      assemble_path (dir_path, entry->d_name, child_path);

      if (0 == stat (child_path, &dir_stat) && S_ISDIR (dir_stat.st_mode)) {
         /* recursively call on child directories */
         paths_index =
            collect_tests_from_dir (paths, child_path, paths_index, max_paths);
      } else if (strstr (entry->d_name, ".json")) {
         /* if this is a JSON test, collect its path */
         assemble_path (dir_path, entry->d_name, paths[paths_index++]);
      }
   }

   closedir (dir);

   return paths_index;
#endif
}

/*
 *-----------------------------------------------------------------------
 *
 * get_bson_from_json_file --
 *
 *        Open the file at @filename and store its contents in a
 *        bson_t. This function assumes that @filename contains a
 *        single JSON object.
 *
 *        NOTE: caller owns returned bson_t and must free it.
 *
 *-----------------------------------------------------------------------
 */
bson_t *
get_bson_from_json_file (char *filename)
{
   FILE *file;
   long length;
   bson_t *data;
   bson_error_t error;
   const char *buffer;

   file = fopen (filename, "rb");
   if (!file) {
      return NULL;
   }

   /* get file length */
   fseek (file, 0, SEEK_END);
   length = ftell (file);
   fseek (file, 0, SEEK_SET);
   if (length < 1) {
      return NULL;
   }

   /* read entire file into buffer */
   buffer = (const char *) bson_malloc0 (length);
   if (fread ((void *) buffer, 1, length, file) != length) {
      abort ();
   }

   fclose (file);
   if (!buffer) {
      return NULL;
   }

   /* convert to bson */
   data = bson_new_from_json ((const uint8_t *) buffer, length, &error);
   if (!data) {
      fprintf (stderr, "Cannot parse %s: %s\n", filename, error.message);
      abort ();
   }

   bson_free ((void *) buffer);

   return data;
}


static int
check_server_version (const bson_t *test)
{
   const char *s;
   char *padded;
   server_version_t test_version, server_version;

   if (bson_has_field (test, "maxServerVersion")) {
      s = bson_lookup_utf8 (test, "maxServerVersion");
      /* s is like "3.0", don't skip if server is 3.0.x but skip 3.1+ */
      padded = bson_strdup_printf ("%s.99", s);
      test_version = test_framework_str_to_version (padded);
      bson_free (padded);
      server_version = test_framework_get_server_version ();

      if (server_version > test_version) {
         if (test_suite_debug_output ()) {
            printf ("      SKIP, maxServerVersion=\"%s\"\n", s);
            fflush (stdout);
         }

         return false;
      }
   }

   if (bson_has_field (test, "minServerVersion")) {
      s = bson_lookup_utf8 (test, "minServerVersion");
      test_version = test_framework_str_to_version (s);
      server_version = test_framework_get_server_version ();

      if (server_version < test_version) {
         if (test_suite_debug_output ()) {
            printf ("      SKIP, minServerVersion=\"%s\"\n", s);
            fflush (stdout);
         }

         return false;
      }
   }

   /* server version is ok, don't skip the test */
   return true;
}


static void
insert_data (mongoc_collection_t *collection, const bson_t *scenario)
{
   mongoc_client_t *client;
   mongoc_collection_t *tmp_collection;
   bool r;
   bson_error_t error;
   mongoc_bulk_operation_t *bulk;
   bson_t documents;
   bson_iter_t iter;
   uint32_t server_id;

   /* clear data using a fresh client not configured with retryWrites etc. */
   client = test_framework_client_new ();
   tmp_collection = mongoc_client_get_collection (
      client, collection->db, collection->collection);
   r = mongoc_collection_delete_many (
      tmp_collection, tmp_bson ("{}"), NULL, NULL, &error);
   ASSERT_OR_PRINT (r, error);
   mongoc_collection_destroy (tmp_collection);
   mongoc_client_destroy (client);

   bulk = mongoc_collection_create_bulk_operation (collection, true, NULL);

   bson_lookup_doc (scenario, "data", &documents);
   bson_iter_init (&iter, &documents);

   while (bson_iter_next (&iter)) {
      bson_t document;
      bson_t opts = BSON_INITIALIZER;

      bson_iter_bson (&iter, &document);
      r = mongoc_bulk_operation_insert_with_opts (
         bulk, &document, &opts, &error);
      ASSERT_OR_PRINT (r, error);

      bson_destroy (&opts);
   }

   server_id = mongoc_bulk_operation_execute (bulk, NULL, &error);
   ASSERT_OR_PRINT (server_id, error);

   bson_destroy (&documents);
   mongoc_bulk_operation_destroy (bulk);
}


static void
add_request_to_bulk (mongoc_bulk_operation_t *bulk, bson_t *request)
{
   const char *name;
   bson_t args;
   bool r;
   bson_t opts = BSON_INITIALIZER;
   bson_error_t error;

   name = bson_lookup_utf8 (request, "name");
   bson_lookup_doc (request, "arguments", &args);

   if (!strcmp (name, "deleteMany")) {
      bson_t filter;

      bson_lookup_doc (&args, "filter", &filter);

      r = mongoc_bulk_operation_remove_many_with_opts (
         bulk, &filter, &opts, &error);
   } else if (!strcmp (name, "deleteOne")) {
      bson_t filter;

      bson_lookup_doc (&args, "filter", &filter);

      r = mongoc_bulk_operation_remove_one_with_opts (
         bulk, &filter, &opts, &error);
   } else if (!strcmp (name, "insertOne")) {
      bson_t document;

      bson_lookup_doc (&args, "document", &document);

      r = mongoc_bulk_operation_insert_with_opts (
         bulk, &document, &opts, &error);
   } else if (!strcmp (name, "replaceOne")) {
      bson_t filter;
      bson_t replacement;

      bson_lookup_doc (&args, "filter", &filter);
      bson_lookup_doc (&args, "replacement", &replacement);

      if (bson_has_field (&args, "upsert")) {
         BSON_APPEND_BOOL (
            &opts, "upsert", _mongoc_lookup_bool (&args, "upsert", false));
      }

      r = mongoc_bulk_operation_replace_one_with_opts (
         bulk, &filter, &replacement, &opts, &error);
   } else if (!strcmp (name, "updateMany")) {
      bson_t filter;
      bson_t update;

      bson_lookup_doc (&args, "filter", &filter);
      bson_lookup_doc (&args, "update", &update);

      if (bson_has_field (&args, "upsert")) {
         BSON_APPEND_BOOL (
            &opts, "upsert", _mongoc_lookup_bool (&args, "upsert", false));
      }

      r = mongoc_bulk_operation_update_many_with_opts (
         bulk, &filter, &update, &opts, &error);
   } else if (!strcmp (name, "updateOne")) {
      bson_t filter;
      bson_t update;

      bson_lookup_doc (&args, "filter", &filter);
      bson_lookup_doc (&args, "update", &update);

      if (bson_has_field (&args, "upsert")) {
         BSON_APPEND_BOOL (
            &opts, "upsert", _mongoc_lookup_bool (&args, "upsert", false));
      }

      r = mongoc_bulk_operation_update_one_with_opts (
         bulk, &filter, &update, &opts, &error);
   } else {
      test_error ("unrecognized request name %s", name);
      abort ();
   }

   ASSERT_OR_PRINT (r, error);

   bson_destroy (&args);
   bson_destroy (&opts);
}


static bson_t *
convert_spec_result_to_bulk_write_result (const bson_t *spec_result)
{
   bson_t *result;
   bson_iter_t iter;

   result = tmp_bson ("{}");

   ASSERT (bson_iter_init (&iter, spec_result));

   while (bson_iter_next (&iter)) {
      /* libmongoc does not report inserted IDs, so ignore those fields */
      if (BSON_ITER_IS_KEY (&iter, "insertedCount")) {
         BSON_APPEND_VALUE (result, "nInserted", bson_iter_value (&iter));
      }
      if (BSON_ITER_IS_KEY (&iter, "deletedCount")) {
         BSON_APPEND_VALUE (result, "nRemoved", bson_iter_value (&iter));
      }
      if (BSON_ITER_IS_KEY (&iter, "matchedCount")) {
         BSON_APPEND_VALUE (result, "nMatched", bson_iter_value (&iter));
      }
      if (BSON_ITER_IS_KEY (&iter, "modifiedCount")) {
         BSON_APPEND_VALUE (result, "nModified", bson_iter_value (&iter));
      }
      if (BSON_ITER_IS_KEY (&iter, "upsertedCount")) {
         BSON_APPEND_VALUE (result, "nUpserted", bson_iter_value (&iter));
      }
      /* convert a single-statement upsertedId result field to a bulk write
       * upsertedIds result field */
      if (BSON_ITER_IS_KEY (&iter, "upsertedId")) {
         bson_t upserted;
         bson_t upsert;

         BSON_APPEND_ARRAY_BEGIN (result, "upserted", &upserted);
         BSON_APPEND_DOCUMENT_BEGIN (&upserted, "0", &upsert);
         BSON_APPEND_INT32 (&upsert, "index", 0);
         BSON_APPEND_VALUE (&upsert, "_id", bson_iter_value (&iter));
         bson_append_document_end (&upserted, &upsert);
         bson_append_array_end (result, &upserted);
      }
      if (BSON_ITER_IS_KEY (&iter, "upsertedIds")) {
         bson_t upserted;
         bson_iter_t inner;
         uint32_t i = 0;

         ASSERT (BSON_ITER_HOLDS_DOCUMENT (&iter));

         /* include the "upserted" field if upsertedIds isn't empty */
         ASSERT (bson_iter_recurse (&iter, &inner));
         while (bson_iter_next (&inner)) {
            i++;
         }

         if (i) {
            i = 0;
            ASSERT (bson_iter_recurse (&iter, &inner));
            BSON_APPEND_ARRAY_BEGIN (result, "upserted", &upserted);

            while (bson_iter_next (&inner)) {
               bson_t upsert;
               const char *keyptr = NULL;
               char key[12];

               bson_uint32_to_string (i++, &keyptr, key, sizeof key);

               BSON_APPEND_DOCUMENT_BEGIN (&upserted, keyptr, &upsert);
               BSON_APPEND_INT32 (
                  &upsert, "index", atoi (bson_iter_key (&inner)));
               BSON_APPEND_VALUE (&upsert, "_id", bson_iter_value (&inner));
               bson_append_document_end (&upserted, &upsert);
            }

            bson_append_array_end (result, &upserted);
         }
      }
   }

   return result;
}


static void
execute_bulk_operation (mongoc_bulk_operation_t *bulk, const bson_t *test)
{
   uint32_t server_id;
   bson_t reply;
   bson_error_t error;

   server_id = mongoc_bulk_operation_execute (bulk, &reply, &error);

   if (_mongoc_lookup_bool (test, "outcome.error", false)) {
      ASSERT (!server_id);
   } else {
      ASSERT_OR_PRINT (server_id, error);
   }

   if (bson_has_field (test, "outcome.result")) {
      bson_t spec_result;
      bson_t *expected_result;

      bson_lookup_doc (test, "outcome.result", &spec_result);
      expected_result = convert_spec_result_to_bulk_write_result (&spec_result);

      ASSERT (match_bson (&reply, expected_result, false));
   }

   bson_destroy (&reply);
}


static bson_t *
create_bulk_write_opts (const bson_t *test, mongoc_client_session_t *session)
{
   bson_t *opts;

   opts = tmp_bson ("{}");

   BSON_APPEND_BOOL (
      opts,
      "ordered",
      _mongoc_lookup_bool (test, "operation.arguments.options.ordered", true));

   if (session) {
      bool r;
      bson_error_t error;

      r = mongoc_client_session_append (session, opts, &error);

      ASSERT_OR_PRINT (r, error);
   }

   return opts;
}


static void
bulk_write (mongoc_collection_t *collection,
            const bson_t *test,
            mongoc_client_session_t *session)
{
   bson_t *opts;
   mongoc_bulk_operation_t *bulk;
   bson_t requests;
   bson_iter_t iter;

   opts = create_bulk_write_opts (test, session);
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, opts);

   bson_lookup_doc (test, "operation.arguments.requests", &requests);
   ASSERT (bson_iter_init (&iter, &requests));

   while (bson_iter_next (&iter)) {
      bson_t request;

      bson_iter_bson (&iter, &request);
      add_request_to_bulk (bulk, &request);
   }

   bson_destroy (&requests);
   execute_bulk_operation (bulk, test);
   mongoc_bulk_operation_destroy (bulk);
}


static void
single_write (mongoc_collection_t *collection,
              const bson_t *test,
              mongoc_client_session_t *session)
{
   bson_t *opts;
   mongoc_bulk_operation_t *bulk;
   bson_t operation;

   opts = create_bulk_write_opts (test, session);
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, opts);

   bson_lookup_doc (test, "operation", &operation);
   add_request_to_bulk (bulk, &operation);

   execute_bulk_operation (bulk, test);

   bson_destroy (&operation);
   mongoc_bulk_operation_destroy (bulk);
}


static mongoc_find_and_modify_opts_t *
create_find_and_modify_opts (const char *name,
                             const bson_t *args,
                             mongoc_client_session_t *session)
{
   mongoc_find_and_modify_opts_t *opts;
   mongoc_find_and_modify_flags_t flags = 0;

   opts = mongoc_find_and_modify_opts_new ();

   if (!strcmp (name, "findOneAndDelete")) {
      flags |= MONGOC_FIND_AND_MODIFY_REMOVE;
   }

   if (!strcmp (name, "findOneAndReplace")) {
      bson_t replacement;
      bson_lookup_doc (args, "replacement", &replacement);
      mongoc_find_and_modify_opts_set_update (opts, &replacement);
   }

   if (!strcmp (name, "findOneAndUpdate")) {
      bson_t update;
      bson_lookup_doc (args, "update", &update);
      mongoc_find_and_modify_opts_set_update (opts, &update);
   }

   if (bson_has_field (args, "sort")) {
      bson_t sort;
      bson_lookup_doc (args, "sort", &sort);
      mongoc_find_and_modify_opts_set_sort (opts, &sort);
   }

   if (_mongoc_lookup_bool (args, "upsert", false)) {
      flags |= MONGOC_FIND_AND_MODIFY_UPSERT;
   }

   if (bson_has_field (args, "returnDocument") &&
       !strcmp ("After", bson_lookup_utf8 (args, "returnDocument"))) {
      flags |= MONGOC_FIND_AND_MODIFY_RETURN_NEW;
   }

   mongoc_find_and_modify_opts_set_flags (opts, flags);

   if (session) {
      bool r;
      bson_t extra = BSON_INITIALIZER;
      bson_error_t error;

      r = mongoc_client_session_append (session, &extra, &error);
      ASSERT_OR_PRINT (r, error);

      ASSERT (mongoc_find_and_modify_opts_append (opts, &extra));
      bson_destroy (&extra);
   }

   return opts;
}


static void
find_and_modify (mongoc_collection_t *collection,
                 bson_t *test,
                 mongoc_client_session_t *session)
{
   const char *name;
   bson_t args;
   bson_t filter;
   mongoc_find_and_modify_opts_t *opts;
   bool r;
   bson_t reply;
   bson_error_t error;

   name = bson_lookup_utf8 (test, "operation.name");
   bson_lookup_doc (test, "operation.arguments", &args);
   bson_lookup_doc (test, "operation.arguments.filter", &filter);

   opts = create_find_and_modify_opts (name, &args, session);
   r = mongoc_collection_find_and_modify_with_opts (
      collection, &filter, opts, &reply, &error);
   mongoc_find_and_modify_opts_destroy (opts);

   if (_mongoc_lookup_bool (test, "outcome.error", false)) {
      ASSERT (!r);
   } else {
      ASSERT_OR_PRINT (r, error);
   }

   if (bson_has_field (test, "outcome.result")) {
      bson_t expected_result;
      bson_t reply_result;

      bson_lookup_doc (test, "outcome.result", &expected_result);
      bson_lookup_doc (&reply, "value", &reply_result);

      ASSERT (match_bson (&reply_result, &expected_result, false));
   }

   bson_destroy (&args);
   bson_destroy (&filter);
   bson_destroy (&reply);
}


static void
insert_many (mongoc_collection_t *collection,
             const bson_t *test,
             mongoc_client_session_t *session)
{
   bson_t *opts;
   mongoc_bulk_operation_t *bulk;
   bson_t documents;
   bson_iter_t iter;

   opts = create_bulk_write_opts (test, session);
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, opts);

   bson_lookup_doc (test, "operation.arguments.documents", &documents);
   ASSERT (bson_iter_init (&iter, &documents));

   while (bson_iter_next (&iter)) {
      bson_t document;
      bool r;
      bson_error_t error;

      bson_iter_bson (&iter, &document);
      r =
         mongoc_bulk_operation_insert_with_opts (bulk, &document, NULL, &error);
      ASSERT_OR_PRINT (r, error);
   }

   execute_bulk_operation (bulk, test);

   bson_destroy (&documents);
   mongoc_bulk_operation_destroy (bulk);
}


static void
check_outcome_collection (mongoc_collection_t *collection, bson_t *test)
{
   bson_t data;
   bson_iter_t iter;
   mongoc_cursor_t *cursor;
   bson_t query = BSON_INITIALIZER;

   bson_lookup_doc (test, "outcome.collection.data", &data);
   ASSERT (bson_iter_init (&iter, &data));

   cursor = mongoc_collection_find_with_opts (collection, &query, NULL, NULL);

   while (bson_iter_next (&iter)) {
      bson_t expected_doc;
      const bson_t *actual_doc;

      bson_iter_bson (&iter, &expected_doc);
      ASSERT_CURSOR_NEXT (cursor, &actual_doc);
      ASSERT (match_bson (actual_doc, &expected_doc, false));
   }

   ASSERT_CURSOR_DONE (cursor);

   bson_destroy (&data);
   bson_destroy (&query);
   mongoc_cursor_destroy (cursor);
}


static void
execute_test (mongoc_collection_t *collection,
              bson_t *test,
              mongoc_client_session_t *session)
{
   uint32_t server_id = 0;
   bson_error_t error;
   const char *op_name;

   if (test_suite_debug_output ()) {
      const char *description = bson_lookup_utf8 (test, "description");
      printf ("  - %s (%s session)\n",
              description,
              session ? "explicit" : "implicit");
      fflush (stdout);
   }

   /* Select a primary for testing */
   server_id = mongoc_topology_select_server_id (
      collection->client->topology, MONGOC_SS_WRITE, NULL, &error);
   ASSERT_OR_PRINT (server_id, error);

   if (bson_has_field (test, "failPoint")) {
      bson_t opts;

      bson_lookup_doc (test, "failPoint", &opts);
      activate_fail_point (collection->client, server_id, &opts);
   }

   op_name = bson_lookup_utf8 (test, "operation.name");

   if (!strcmp (op_name, "bulkWrite")) {
      bulk_write (collection, test, session);
   } else if (!strcmp (op_name, "deleteOne") ||
              !strcmp (op_name, "insertOne") ||
              !strcmp (op_name, "replaceOne") ||
              !strcmp (op_name, "updateOne")) {
      single_write (collection, test, session);
   } else if (!strcmp (op_name, "findOneAndDelete") ||
              !strcmp (op_name, "findOneAndReplace") ||
              !strcmp (op_name, "findOneAndUpdate")) {
      find_and_modify (collection, test, session);
   } else if (!strcmp (op_name, "insertMany")) {
      insert_many (collection, test, session);
   } else {
      test_error ("unrecognized operation name %s", op_name);
      abort ();
   }

   if (bson_has_field (test, "outcome.collection")) {
      check_outcome_collection (collection, test);
   }

   deactivate_fail_point (collection->client, server_id);
}


/*
 *-----------------------------------------------------------------------
 *
 * activate_fail_point --
 *
 *      Activate the onPrimaryTransactionalWrite failpoint on a server
 *      with id @server_id, passing options @opts.
 *
 *-----------------------------------------------------------------------
 */

void
activate_fail_point (mongoc_client_t *client,
                     uint32_t server_id,
                     const bson_t *opts)
{
   bson_t *command;
   bool r;
   bson_error_t error;

   command = tmp_bson ("{'configureFailPoint': 'onPrimaryTransactionalWrite'}");
   bson_copy_to_excluding_noinit (opts, command, "configureFailPoint", NULL);

   r = mongoc_client_command_simple_with_server_id (
      client, "admin", command, NULL, server_id, NULL, &error);
   ASSERT_OR_PRINT (r, error);
}


/*
 *-----------------------------------------------------------------------
 *
 * deactivate_fail_point --
 *
 *      Deactive the onPrimaryTransactionalWrite.
 *
 *-----------------------------------------------------------------------
 */
void
deactivate_fail_point (mongoc_client_t *client, uint32_t server_id)
{
   bson_t *command;
   bool r;
   bson_error_t error;

   command = tmp_bson (
      "{'configureFailPoint': 'onPrimaryTransactionalWrite', 'mode': 'off'}");

   r = mongoc_client_command_simple_with_server_id (
      client, "admin", command, NULL, server_id, NULL, &error);
   ASSERT_OR_PRINT (r, error);
}


static void
set_uri_opts_from_bson (mongoc_uri_t *uri, const bson_t *opts)
{
   bson_iter_t iter;

   BSON_ASSERT (bson_iter_init (&iter, opts));
   while (bson_iter_next (&iter)) {
      if (BSON_ITER_HOLDS_UTF8 (&iter)) {
         mongoc_uri_set_option_as_utf8 (
            uri, bson_iter_key (&iter), bson_iter_utf8 (&iter, NULL));
      } else if (BSON_ITER_HOLDS_BOOL (&iter)) {
         mongoc_uri_set_option_as_bool (
            uri, bson_iter_key (&iter), bson_iter_bool (&iter));
      } else if (BSON_ITER_HOLDS_NUMBER (&iter)) {
         mongoc_uri_set_option_as_int32 (
            uri, bson_iter_key (&iter), bson_iter_int32 (&iter));
      } else {
         MONGOC_ERROR ("Unsupported clientOptions type for field \"%s\" in %s",
                       bson_iter_key (&iter),
                       bson_as_json (opts, NULL));
         abort ();
      }
   }
}

/*
 *-----------------------------------------------------------------------
 *
 * run_json_general_test --
 *
 *      Run a JSON test scenario from the CRUD, Command Monitoring,
 *      Retryable Writes, or Transactions Spec.
 *
 *-----------------------------------------------------------------------
 */
void
run_json_general_test (const bson_t *scenario, bool explicit_session)
{
   bson_iter_t scenario_iter;
   bson_iter_t tests_iter;

   ASSERT (scenario);

   if (!check_server_version (scenario)) {
      return;
   }

   ASSERT (bson_iter_init_find (&scenario_iter, scenario, "tests"));
   ASSERT (BSON_ITER_HOLDS_ARRAY (&scenario_iter));
   ASSERT (bson_iter_recurse (&scenario_iter, &tests_iter));

   while (bson_iter_next (&tests_iter)) {
      bson_t test;
      bson_iter_t client_opts_iter;
      mongoc_uri_t *uri;
      mongoc_client_t *client;
      mongoc_client_session_t *session = NULL;
      mongoc_collection_t *collection;
      uint32_t server_id;
      bson_error_t error;

      ASSERT (BSON_ITER_HOLDS_DOCUMENT (&tests_iter));
      bson_iter_bson (&tests_iter, &test);

      uri = test_framework_get_uri ();
      if (bson_iter_init_find (&client_opts_iter, &test, "clientOptions")) {
         bson_t client_opts;

         ASSERT (BSON_ITER_HOLDS_DOCUMENT (&tests_iter));
         bson_iter_bson (&client_opts_iter, &client_opts);
         set_uri_opts_from_bson (uri, &client_opts);
      }

      client = mongoc_client_new_from_uri (uri);
      test_framework_set_ssl_opts (client);
      /* reconnect right away after the failpoint causes a disconnect */
      client->topology->min_heartbeat_frequency_msec = 0;
      mongoc_uri_destroy (uri);

      /* clean up in case a previous test aborted */
      server_id = mongoc_topology_select_server_id (
         client->topology, MONGOC_SS_WRITE, NULL, &error);
      ASSERT_OR_PRINT (server_id, error);
      deactivate_fail_point (client, server_id);

      if (explicit_session) {
         session = mongoc_client_start_session (client, NULL, &error);
         ASSERT_OR_PRINT (session, error);
      }

      collection = get_test_collection (client, "retryable_writes");

      insert_data (collection, scenario);
      execute_test (collection, &test, session);

      if (session) {
         mongoc_client_session_destroy (session);
      }

      mongoc_collection_destroy (collection);
      mongoc_client_destroy (client);
      bson_destroy (&test);
   }
}

/*
 *-----------------------------------------------------------------------
 *
 * install_json_test_suite --
 *
 *      Given a path to a directory containing JSON tests, import each
 *      test into a BSON blob and call the provided callback for
 *      evaluation.
 *
 *      It is expected that the callback will BSON_ASSERT on failure, so if
 *      callback returns quietly the test is considered to have passed.
 *
 *-----------------------------------------------------------------------
 */
void
_install_json_test_suite_with_check (TestSuite *suite,
                                     const char *dir_path,
                                     test_hook callback,
                                     ...)
{
   char test_paths[MAX_NUM_TESTS][MAX_TEST_NAME_LENGTH];
   int num_tests;
   int i;
   bson_t *test;
   char *skip_json;
   char *ext;
   va_list ap;

   num_tests =
      collect_tests_from_dir (&test_paths[0], dir_path, 0, MAX_NUM_TESTS);

   for (i = 0; i < num_tests; i++) {
      test = get_bson_from_json_file (test_paths[i]);
      skip_json = COALESCE (strstr (test_paths[i], "/json"),
                            strstr (test_paths[i], "\\json"));
      BSON_ASSERT (skip_json);
      skip_json += strlen ("/json");
      ext = strstr (skip_json, ".json");
      BSON_ASSERT (ext);
      ext[0] = '\0';

      /* list of "check" functions that decide whether to skip the test */
      va_start (ap, callback);
      _V_TestSuite_AddFull (suite,
                            skip_json,
                            (void (*) (void *)) callback,
                            (void (*) (void *)) bson_destroy,
                            test,
                            ap);

      va_end (ap);
   }
}


/*
 *-----------------------------------------------------------------------
 *
 * install_json_test_suite --
 *
 *      Given a path to a directory containing JSON tests, import each
 *      test into a BSON blob and call the provided callback for
 *      evaluation.
 *
 *      It is expected that the callback will BSON_ASSERT on failure, so if
 *      callback returns quietly the test is considered to have passed.
 *
 *-----------------------------------------------------------------------
 */
void
install_json_test_suite (TestSuite *suite,
                         const char *dir_path,
                         test_hook callback)
{
   install_json_test_suite_with_check (
      suite, dir_path, callback, TestSuite_CheckLive);
}
