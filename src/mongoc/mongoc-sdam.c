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
#include "mongoc-sdam-private.h"
#include "mongoc-trace.h"

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_new --
 *
 *       Creates and returns a new SDAM object.
 *
 *       NOTE: use _mongoc_sdam_grab() and _mongoc_sdam_release() to
 *       manage the lifetime of this object. Do not attempt to use this
 *       object before calling _mongoc_sdam_grab().
 *
 * Returns:
 *       A new SDAM object.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */
mongoc_sdam_t *
_mongoc_sdam_new (const mongoc_uri_t *uri)
{
   mongoc_sdam_t *sdam;

   bson_return_val_if_fail(uri, NULL);

   sdam = bson_malloc0(sizeof *sdam);
   sdam->users = 0;
   sdam->uri = uri;

   _mongoc_topology_description_init(&sdam->topology);

   // TODO, hook up scanner once ready

   return sdam;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_grab --
 *
 *       Increments the users counter in @sdam.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */
void
_mongoc_sdam_grab (mongoc_sdam_t *sdam)
{
   bson_return_if_fail(sdam);

   sdam->users++;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_release --
 *
 *       Decrements number of users of @sdam.  If number falls to < 1,
 *       destroys this mongoc_sdam_t.  Treat this as destroy, and do not
 *       attempt to use @sdam after calling.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       May destroy @sdam.
 *
 *-------------------------------------------------------------------------
 */
void
_mongoc_sdam_release (mongoc_sdam_t *sdam)
{
   bson_return_if_fail(sdam);

   sdam->users--;
   if (sdam->users < 1) {
      _mongoc_sdam_destroy(sdam);
   }
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_destroy --
 *
 *       Free the memory associated with this sdam object.
 *
 *       NOTE: users should not call this directly; rather, use
 *       _mongoc_sdam_grab() and _mongoc_sdam_release() to indicate
 *       posession of this object.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @sdam will be cleaned up. Any users using this object without
 *       having called _mongoc_sdam_grab() will find themselves with
 *       a null pointer.
 *
 *-------------------------------------------------------------------------
 */
void
_mongoc_sdam_destroy (mongoc_sdam_t *sdam)
{
   _mongoc_topology_description_destroy(&sdam->topology);
   bson_free(sdam);
   sdam = NULL;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_select --
 *
 *       Selects a server description for an operation based on @optype
 *       and @read_prefs.
 *
 *       NOTE: this method returns a copy of the original server
 *       description. Callers must own and clean up this copy.
 *
 * Returns:
 *       A mongoc_server_description_t, or NULL on failure, in which case
 *       @error will be set.
 *
 * Side effects:
 *       @error may be set.
 *
 *-------------------------------------------------------------------------
 */
mongoc_server_description_t *
_mongoc_sdam_select (mongoc_sdam_t *sdam,
                     mongoc_ss_optype_t optype,
                     const mongoc_read_prefs_t *read_prefs,
                     bson_error_t *error)
{
   mongoc_server_description_t *selected_server;

   bson_return_val_if_fail(sdam, NULL);

   selected_server = _mongoc_topology_description_select(&sdam->topology,
                                                         optype,
                                                         read_prefs,
                                                         error);

   return _mongoc_server_description_new_copy(selected_server);
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_server_by_id --
 *
 *       Get the server description for @id, if that server is present
 *       in @description. Otherwise, return NULL.
 *
 *       NOTE: this method returns a copy of the original server
 *       description. Callers must own and clean up this copy.
 *
 * Returns:
 *       A mongoc_server_description_t, or NULL.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */

mongoc_server_description_t *
_mongoc_sdam_server_by_id (mongoc_sdam_t *sdam, uint32_t id)
{
   return _mongoc_server_description_new_copy(
             _mongoc_topology_description_server_by_id(
                 &sdam->topology, id));
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_force_scan --
 *
 *       Force an immediate scan of the topology.
 *
 *       Scan will not run if there is already a scan in progress.
 *       If MONGOC_SDAM_HEARTBEAT_FREQUENCY_MS milliseconds have not
 *       passed since the previous scan, this scan will wait to run until
 *       that time interval has elapsed.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       May affect topology state.
 *       Depending on configuration, may issue blocking network calls.
 *
 *-------------------------------------------------------------------------
 */
void
_mongoc_sdam_force_scan (mongoc_sdam_t *sdam)
{
   // TODO once scanner is done
   return;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_ismaster_callback --
 *
 *-------------------------------------------------------------------------
 */
void
_mongoc_sdam_ismaster_callback (mongoc_sdam_t *sdam, const bson_t *ismaster)
{
   // TODO once scanner is done
   return;
}
