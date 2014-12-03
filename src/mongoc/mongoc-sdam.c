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
   // TODO
   mongoc_sdam_t *sdam;

   sdam = bson_malloc0(sizeof *sdam);
   sdam->users = 0;
   sdam->uri = uri;

   _mongoc_topology_description_init(&sdam->topology);

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
 *-------------------------------------------------------------------------
 */
void
_mongoc_sdam_destroy (mongoc_sdam_t *sdam)
{
   _mongoc_topology_description_destroy(&sdam->topology);
   bson_free(sdam);
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_select --
 *
 *-------------------------------------------------------------------------
 */
mongoc_server_description_t *
_mongoc_sdam_select (mongoc_sdam_t *sdam, mongoc_ss_optype_t optype,
                     const mongoc_read_prefs_t *read_prefs, bson_error_t *error)
{
   return _mongoc_topology_description_select(&sdam->topology,
                                              optype,
                                              read_prefs,
                                              error);
}

/*
 *-------------------------------------------------------------------------
 *-------------------------------------------------------------------------
 */
void
_mongoc_sdam_force_scan (mongoc_sdam_t *sdam)
{
   // TODO SDAM
   return;
}

/*
 *-------------------------------------------------------------------------
 *-------------------------------------------------------------------------
 */
void
_mongoc_sdam_ismaster_callback (mongoc_sdam_t *sdam, const bson_t  *ismaster)
{
   // TODO SDAM
   return;
}
