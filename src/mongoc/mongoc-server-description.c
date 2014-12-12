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

#include "mongoc-host-list.h"
#include "mongoc-host-list-private.h"
#include "mongoc-server-description.h"
#include "mongoc-trace.h"
#include "mongoc-uri.h"

#define MIN_WIRE_VERSION 0
#define MAX_WIRE_VERSION 3

#define ALPHA 0.2

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_server_description_init --
 *
 *       Initialize a new server_description_t.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
void
_mongoc_server_description_init (mongoc_server_description_t *description,
                                 const char                  *address,
                                 uint32_t                     id)
{
   ENTRY;

   bson_return_if_fail(description);
   bson_return_if_fail(address);

   memset (description, 0, sizeof *description);

   description->id = id;
   description->type = MONGOC_SERVER_UNKNOWN;
   description->round_trip_time = -1;

   if (!_mongoc_host_list_from_string(&description->host, address)) {
      MONGOC_WARNING("Failed to parse uri for %s", address);
      return;
   }

   bson_init (&description->last_is_master);

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_server_description_destroy --
 *
 *       Destroy allocated resources within @description.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
void
_mongoc_server_description_destroy (mongoc_server_description_t *description)
{
   ENTRY;

   BSON_ASSERT(description);

   bson_destroy (&description->last_is_master);

   bson_free(description);

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_server_description_has_rs_member --
 *
 *       Return true if this address is included in server's list of rs
 *       members, false otherwise.
 *
 * Returns:
 *       true, false
 *
 * Side effects:
 *       None
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_server_description_has_rs_member(mongoc_server_description_t *server,
                                         const char                  *address)
{
   bson_iter_t member_iter;
   const bson_t *rs_members[3];
   int i;

   if (server->type != MONGOC_SERVER_UNKNOWN) {
      rs_members[0] = &server->hosts;
      rs_members[1] = &server->arbiters;
      rs_members[2] = &server->passives;

      for (i = 0; i < 3; i++) {
         bson_iter_init (&member_iter, rs_members[i]);

         while (bson_iter_next (&member_iter)) {
            if (strcmp (address, bson_iter_utf8 (&member_iter, NULL)) == 0) {
               return true;
            }
         }
      }
   }

   return false;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_server_description_set_state --
 *
 *       Change the state of this server.
 *
 * Returns:
 *       true, false
 *
 * Side effects:
 *       None
 *
 *--------------------------------------------------------------------------
 */

void _mongoc_server_description_set_state (mongoc_server_description_t *description,
                                           mongoc_server_description_type_t type)
{
   description->type = type;
   // TODO SDAM unblock waiters? Can waiters wait on particular server?
}


/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_server_description_update_rtt --
 *
 *       Update this server's rtt calculation using an exponentially-
 *       weighted moving average formula.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Changes this server description's rtt.
 *
 *-------------------------------------------------------------------------
 */

void
_mongoc_server_description_update_rtt (mongoc_server_description_t *server,
                                       int64_t new_time)
{
   // TODO SS implement
   return;
}

void
_mongoc_server_description_handle_ismaster (
   mongoc_server_description_t   *sd,
   const bson_t                  *reply,
   int64_t                        rtt_msec,
   bson_error_t                  *error)
{
   bson_iter_t iter;
   bool is_master = false;
   bool is_shard = false;
   bool is_secondary = false;
   bool is_arbiter = false;
   /*
   bool is_passive = false;
   bool is_hidden = false;
   */
   bool is_replicaset = false;
   const uint8_t *bytes;
   uint32_t len;

   bson_destroy (&sd->last_is_master);
   bson_copy_to (reply, &sd->last_is_master);

   bson_iter_init (&iter, &sd->last_is_master);

   memset (&sd->set_name, sizeof (*sd) - ((char*)&sd->set_name - (char*)sd), 0);

   while (bson_iter_next (&iter)) {
      /* TODO: do we need to handle maxBsonObjSize */
      if (strcmp ("ismaster", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_BOOL (&iter)) goto ERROR;
         is_master = bson_iter_bool (&iter);
      } else if (strcmp ("maxMessageSizeBytes", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto ERROR;
         sd->max_write_batch_size = bson_iter_int32 (&iter);
      } else if (strcmp ("minWireVersion", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto ERROR;
         sd->min_wire_version = bson_iter_int32 (&iter);
      } else if (strcmp ("maxWireVersion", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto ERROR;
         sd->max_wire_version = bson_iter_int32 (&iter);
      } else if (strcmp ("msg", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_BOOL (&iter)) goto ERROR;
         is_shard = bson_iter_bool (&iter);
      } else if (strcmp ("setName", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_UTF8 (&iter)) goto ERROR;

         sd->set_name = bson_iter_utf8 (&iter, NULL);
      } else if (strcmp ("secondary", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_BOOL (&iter)) goto ERROR;
         is_secondary = bson_iter_bool (&iter);
      } else if (strcmp ("hosts", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_ARRAY (&iter)) goto ERROR;
         bson_iter_array (&iter, &len, &bytes);
         bson_init_static (&sd->hosts, bytes, len);
      } else if (strcmp ("passives", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_ARRAY (&iter)) goto ERROR;
         bson_iter_array (&iter, &len, &bytes);
         bson_init_static (&sd->passives, bytes, len);
      } else if (strcmp ("arbiters", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_ARRAY (&iter)) goto ERROR;
         bson_iter_array (&iter, &len, &bytes);
         bson_init_static (&sd->arbiters, bytes, len);
      } else if (strcmp ("primary", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_UTF8 (&iter)) goto ERROR;
         sd->current_primary = bson_iter_utf8 (&iter, NULL);
      } else if (strcmp ("arbiterOnly", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_BOOL (&iter)) goto ERROR;
         is_arbiter = bson_iter_bool (&iter);
         /*
      } else if (strcmp ("passive", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_BOOL (&iter)) goto ERROR;
         is_passive = bson_iter_bool (&iter);
      } else if (strcmp ("hidden", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_BOOL (&iter)) goto ERROR;
         is_hidden = bson_iter_bool (&iter);
         */
      } else if (strcmp ("isreplicaset", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_BOOL (&iter)) goto ERROR;
         is_replicaset = bson_iter_bool (&iter);
      } else if (strcmp ("tags", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_DOCUMENT (&iter)) goto ERROR;
         bson_iter_document (&iter, &len, &bytes);
         bson_init_static (&sd->tags, bytes, len);
      } else if (strcmp ("me", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_UTF8 (&iter)) goto ERROR;
         sd->connection_address = bson_iter_utf8 (&iter, NULL);
      }
   }

   if (is_shard) {
      sd->type = MONGOC_SERVER_MONGOS;
   } else if (sd->set_name) {
      if (is_master) {
         sd->type = MONGOC_SERVER_RS_PRIMARY;
      } else if (is_secondary) {
         sd->type = MONGOC_SERVER_RS_SECONDARY;
      } else if (is_arbiter) {
         sd->type = MONGOC_SERVER_RS_ARBITER;
      } else {
         sd->type = MONGOC_SERVER_RS_OTHER;
      }
   } else if (is_replicaset) {
      sd->type = MONGOC_SERVER_RS_GHOST;
   } else if (is_master) {
      sd->type = MONGOC_SERVER_STANDALONE;
   } else {
      goto ERROR;
   }

   if (sd->round_trip_time == -1) {
      sd->round_trip_time = rtt_msec;
   } else {
      /* calculate round trip time as an exponentially weighted moving average
       * with a weigt of ALPHA */
      sd->round_trip_time = ALPHA * rtt_msec + (1 - ALPHA) * sd->round_trip_time;
   }

   return;

ERROR:

   sd->type = MONGOC_SERVER_UNKNOWN;
   sd->round_trip_time = -1;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_server_description_new_copy --
 *
 *-------------------------------------------------------------------------
 */
mongoc_server_description_t *
_mongoc_server_description_new_copy (const mongoc_server_description_t *description)
{
   mongoc_server_description_t *copy;

   bson_return_val_if_fail(description, NULL);

   copy = bson_malloc0(sizeof (*copy));

   copy->id = description->id;
   memcpy (&copy->host, &description->host, sizeof (copy->host));
   copy->round_trip_time = -1;
   bson_init (&copy->last_is_master);

   _mongoc_server_description_handle_ismaster (copy, &description->last_is_master,
                                               description->round_trip_time, NULL);

   return copy;
}
