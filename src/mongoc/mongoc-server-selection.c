/*
 * Copyright 2014 MongoDB, Inc.
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

#include "mongoc-error.h"
#include "mongoc-server-selection.h"
#include "mongoc-trace.h"

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_ss_select_within_window --
 *
 *       Given an array of suitable servers, choose one from within
 *       the latency window and return its description.
 *
 * Returns:
 *       A server description, or NULL upon failure
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */

static mongoc_server_description_t *
_mongoc_ss_select_within_window (mongoc_array_t *suitable_servers)
{
   // TODO SS implement
   return NULL;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_ss_suitable_servers --
 *
 *       Return an array of suitable server descriptions for this
 *       operation and read preference.
 *
 * Returns:
 *       Array of server descriptions, or NULL upon failure.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */

static mongoc_array_t *
_mongoc_ss_suitable_servers (mongoc_ss_optype_t optype,
                             mongoc_topology_description_t *topology,
                             const mongoc_read_prefs_t *read_pref)
{
   // TODO SS implement
   return NULL;
}


/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_ss_select --
 *
 *       Return a server description of a node that is appropriate for
 *       the given read preference and operation type.
 *
 * Returns:
 *       Selected server description, or NULL upon failure.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */

mongoc_server_description_t *
_mongoc_ss_select (mongoc_ss_optype_t optype,
                   mongoc_topology_description_t *topology,
                   const mongoc_read_prefs_t *read_pref,
                   bson_error_t *error /* OUT */)
{
   /* timeout calculations in microseconds */
   int64_t start_time = bson_get_monotonic_time();
   int64_t now = bson_get_monotonic_time();
   int64_t timeout = (MONGOC_SS_DEFAULT_TIMEOUT_MS * 1000UL); // TODO SS use client-set timeout if available
   mongoc_array_t *suitable_servers = NULL;

   ENTRY;

   if (!topology->compatible) {
      bson_set_error(error,
                     MONGOC_ERROR_SS,
                     MONGOC_ERROR_SS_TIMEOUT,
                     "Invalid topology wire version range");
      RETURN(NULL);
   }

   // TODO SS:
   // we should also timeout when minHearbeatFrequencyMS ms have passed, per check.
   do {
      suitable_servers = _mongoc_ss_suitable_servers(optype, topology, read_pref);
      if (!suitable_servers) {
         RETURN( _mongoc_ss_select_within_window(suitable_servers));
      }
      // TODO SS request scan, and wait
      now = bson_get_monotonic_time();

      // TODO wait on condition variable
   } while (start_time + timeout < now);

   bson_set_error(error,
                  MONGOC_ERROR_SS,
                  MONGOC_ERROR_SS_TIMEOUT,
                  "Could not find a suitable server");
   RETURN(NULL);
}
