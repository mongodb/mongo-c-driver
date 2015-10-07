/*
 * Copyright 2013 MongoDB, Inc.
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


#include "mongoc-read-prefs.h"
#include "mongoc-read-prefs-private.h"


mongoc_read_prefs_t *
mongoc_read_prefs_new (mongoc_read_mode_t mode)
{
   mongoc_read_prefs_t *read_prefs;

   read_prefs = (mongoc_read_prefs_t *)bson_malloc0(sizeof *read_prefs);
   read_prefs->mode = mode;
   bson_init(&read_prefs->tags);

   return read_prefs;
}


mongoc_read_mode_t
mongoc_read_prefs_get_mode (const mongoc_read_prefs_t *read_prefs)
{
   BSON_ASSERT (read_prefs);
   return read_prefs->mode;
}


void
mongoc_read_prefs_set_mode (mongoc_read_prefs_t *read_prefs,
                            mongoc_read_mode_t   mode)
{
   BSON_ASSERT (read_prefs);
   BSON_ASSERT (mode <= MONGOC_READ_NEAREST);

   read_prefs->mode = mode;
}


const bson_t *
mongoc_read_prefs_get_tags (const mongoc_read_prefs_t *read_prefs)
{
   BSON_ASSERT (read_prefs);
   return &read_prefs->tags;
}


void
mongoc_read_prefs_set_tags (mongoc_read_prefs_t *read_prefs,
                            const bson_t        *tags)
{
   BSON_ASSERT (read_prefs);

   bson_destroy(&read_prefs->tags);

   if (tags) {
      bson_copy_to(tags, &read_prefs->tags);
   } else {
      bson_init(&read_prefs->tags);
   }
}


void
mongoc_read_prefs_add_tag (mongoc_read_prefs_t *read_prefs,
                           const bson_t        *tag)
{
   bson_t empty = BSON_INITIALIZER;
   char str[16];
   int key;

   BSON_ASSERT (read_prefs);

   key = bson_count_keys (&read_prefs->tags);
   bson_snprintf (str, sizeof str, "%d", key);

   if (tag) {
      bson_append_document (&read_prefs->tags, str, -1, tag);
   } else {
      bson_append_document (&read_prefs->tags, str, -1, &empty);
   }
}


bool
mongoc_read_prefs_is_valid (const mongoc_read_prefs_t *read_prefs)
{
   BSON_ASSERT (read_prefs);

   /*
    * Tags are not supported with PRIMARY mode.
    */
   if (read_prefs->mode == MONGOC_READ_PRIMARY) {
      if (!bson_empty(&read_prefs->tags)) {
         return false;
      }
   }

   return true;
}


void
mongoc_read_prefs_destroy (mongoc_read_prefs_t *read_prefs)
{
   if (read_prefs) {
      bson_destroy(&read_prefs->tags);
      bson_free(read_prefs);
   }
}


mongoc_read_prefs_t *
mongoc_read_prefs_copy (const mongoc_read_prefs_t *read_prefs)
{
   mongoc_read_prefs_t *ret = NULL;

   if (read_prefs) {
      ret = mongoc_read_prefs_new(read_prefs->mode);
      bson_copy_to(&read_prefs->tags, &ret->tags);
   }

   return ret;
}


/* Server Selection Spec: "When any $ modifier is used, including the
 * $readPreference modifier, the query MUST be provided using the $query
 * modifier".
 *
 * This applies to commands, too.
 */
static void
_prep_for_read_pref_modifier (bson_t *query_bson)
{
   bson_t tmp;

   BSON_ASSERT (query_bson);

   if (bson_empty (query_bson) || bson_has_field (query_bson, "$query")) {
      return;
   }

   bson_copy_to (query_bson, &tmp);
   bson_reinit (query_bson);
   bson_append_document (query_bson, "$query", 6, &tmp);
   bson_destroy (&tmp);
}


static const char *
_get_read_mode_string (mongoc_read_mode_t mode)
{
   switch (mode) {
   case MONGOC_READ_PRIMARY:
      return "primary";
   case MONGOC_READ_PRIMARY_PREFERRED:
      return "primaryPreferred";
   case MONGOC_READ_SECONDARY:
      return "secondary";
   case MONGOC_READ_SECONDARY_PREFERRED:
      return "secondaryPreferred";
   case MONGOC_READ_NEAREST:
      return "nearest";
   default:
      return "";
   }
}


/* Update the RPC with the read prefs, following Server Selection Spec.
 * The driver must have discovered the server is a mongos.
 */
static void
_apply_read_preferences_mongos (mongoc_read_prefs_t *read_prefs,
                                bson_t *query_bson,
                                mongoc_rpc_query_t *query_rpc)  /* IN  / OUT */
{
   mongoc_read_mode_t mode = MONGOC_READ_PRIMARY;
   const bson_t *tags = NULL;
   bson_t child;
   const char *mode_str;

   if (read_prefs) {
      mode = mongoc_read_prefs_get_mode (read_prefs);
      tags = mongoc_read_prefs_get_tags (read_prefs);
   }

   /* Server Selection Spec says:
    *
    * For mode 'primary', drivers MUST NOT set the slaveOK wire protocol flag
    *   and MUST NOT use $readPreference
    *
    * For mode 'secondary', drivers MUST set the slaveOK wire protocol flag and
    *   MUST also use $readPreference
    *
    * For mode 'primaryPreferred', drivers MUST set the slaveOK wire protocol
    *   flag and MUST also use $readPreference
    *
    * For mode 'secondaryPreferred', drivers MUST set the slaveOK wire protocol
    *   flag. If the read preference contains a non-empty tag_sets parameter,
    *   drivers MUST use $readPreference; otherwise, drivers MUST NOT use
    *   $readPreference
    *
    * For mode 'nearest', drivers MUST set the slaveOK wire protocol flag and
    *   MUST also use $readPreference
    */
   if (mode == MONGOC_READ_SECONDARY_PREFERRED && bson_empty0 (tags)) {
      query_rpc->flags |= MONGOC_QUERY_SLAVE_OK;

   } else if (mode != MONGOC_READ_PRIMARY) {
      query_rpc->flags |= MONGOC_QUERY_SLAVE_OK;

      _prep_for_read_pref_modifier (query_bson);
      bson_append_document_begin (query_bson, "$readPreference",
                                  15, &child);
      mode_str = _get_read_mode_string (mode);
      bson_append_utf8 (&child, "mode", 4, mode_str, -1);
      if (!bson_empty0 (tags)) {
         bson_append_array (&child, "tags", 4, tags);
      }

      bson_append_document_end (query_bson, &child);
   }

   query_rpc->query = bson_get_data (query_bson);
}


/* Update rpc->query and flags from read prefs, following Server Selection Spec.
 * Called after selecting a server: topology and server type must be known.
 */
void
apply_read_preferences (mongoc_read_prefs_t *read_prefs,
                        mongoc_topology_description_type_t topology_type,
                        mongoc_server_description_type_t server_type,
                        bson_t *query_bson,
                        mongoc_rpc_query_t *query_rpc)         /* IN  / OUT */
{
   switch (topology_type) {
   case MONGOC_TOPOLOGY_SINGLE:
      if (server_type == MONGOC_SERVER_MONGOS) {
         _apply_read_preferences_mongos (read_prefs, query_bson, query_rpc);
         return;
      } else {
         /* Server Selection Spec: for topology type single and server types
          * besides mongos, "clients MUST always set the slaveOK wire protocol
          * flag on reads to ensure that any server type can handle the
          * request."
          */
         query_rpc->flags |= MONGOC_QUERY_SLAVE_OK;
      }

      break;

   case MONGOC_TOPOLOGY_RS_NO_PRIMARY:
   case MONGOC_TOPOLOGY_RS_WITH_PRIMARY:
      if (read_prefs) {
         /* Server Selection Spec: for RS topology types, "For all read
          * preferences modes except primary, clients MUST set the slaveOK wire
          * protocol flag to ensure that any suitable server can handle the
          * request. Clients MUST  NOT set the slaveOK wire protocol flag if the
          * read preference mode is primary.
          */
         if (mongoc_read_prefs_get_mode (read_prefs) != MONGOC_READ_PRIMARY) {
            query_rpc->flags |= MONGOC_QUERY_SLAVE_OK;
         }
      }

      break;

   case MONGOC_TOPOLOGY_SHARDED:
      _apply_read_preferences_mongos (read_prefs, query_bson, query_rpc);
      return;

   case MONGOC_TOPOLOGY_UNKNOWN:
   case MONGOC_TOPOLOGY_DESCRIPTION_TYPES:
   default:
      /* must not call _apply_read_preferences with unknown topology type */
      BSON_ASSERT (false);
      break;
   }

   /* we haven't called _apply_read_preferences_mongos, must set query */
   query_rpc->query = bson_get_data (query_bson);
}
