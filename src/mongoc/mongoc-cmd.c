/*
 * Copyright 2017 MongoDB, Inc.
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


#include "mongoc-cmd-private.h"
#include "mongoc-read-prefs-private.h"
#include "mongoc-trace-private.h"
#include "mongoc-client-private.h"
#include "mongoc-write-concern-private.h"
/* For strcasecmp on Windows */
#include "mongoc-util-private.h"


void
mongoc_cmd_parts_init (mongoc_cmd_parts_t *parts,
                       mongoc_client_t *client,
                       const char *db_name,
                       mongoc_query_flags_t user_query_flags,
                       const bson_t *command_body)
{
   parts->body = command_body;
   parts->user_query_flags = user_query_flags;
   parts->read_prefs = NULL;
   parts->is_write_command = false;
   parts->client = client;
   bson_init (&parts->extra);
   bson_init (&parts->assembled_body);

   parts->assembled.db_name = db_name;
   parts->assembled.command = NULL;
   parts->assembled.query_flags = MONGOC_QUERY_NONE;
   parts->assembled.payload_identifier = NULL;
   parts->assembled.payload = NULL;
   parts->assembled.session = NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cmd_parts_append_opts --
 *
 *       Take an iterator over user-supplied options document and append the
 *       options to @parts->command_extra, taking the selected server's max
 *       wire version into account.
 *
 * Return:
 *       True if the options were successfully applied. If any options are
 *       invalid, returns false and fills out @error. In that case @parts is
 *       invalid and must not be used.
 *
 * Side effects:
 *       May partly apply options before returning an error.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_cmd_parts_append_opts (mongoc_cmd_parts_t *parts,
                              bson_iter_t *iter,
                              int max_wire_version,
                              bson_error_t *error)
{
   bool is_fam;

   ENTRY;

   /* not yet assembled */
   BSON_ASSERT (!parts->assembled.command);

   is_fam =
      !strcasecmp (_mongoc_get_command_name (parts->body), "findandmodify");

   while (bson_iter_next (iter)) {
      if (BSON_ITER_IS_KEY (iter, "collation")) {
         if (max_wire_version < WIRE_VERSION_COLLATION) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                            "The selected server does not support collation");
            RETURN (false);
         }

      } else if (BSON_ITER_IS_KEY (iter, "writeConcern")) {
         if (!_mongoc_write_concern_iter_is_valid (iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "Invalid writeConcern");
            RETURN (false);
         }

         if ((is_fam && max_wire_version < WIRE_VERSION_FAM_WRITE_CONCERN) ||
             (!is_fam && max_wire_version < WIRE_VERSION_CMD_WRITE_CONCERN)) {
            continue;
         }

      } else if (BSON_ITER_IS_KEY (iter, "readConcern")) {
         if (max_wire_version < WIRE_VERSION_READ_CONCERN) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                            "The selected server does not support readConcern");
            RETURN (false);
         }
      } else if (BSON_ITER_IS_KEY (iter, "sessionId")) {
         if (!_mongoc_client_session_from_iter (
                parts->client, iter, &parts->assembled.session, error)) {
            RETURN (false);
         }

         continue;
      } else if (BSON_ITER_IS_KEY (iter, "serverId") ||
                 BSON_ITER_IS_KEY (iter, "maxAwaitTimeMS")) {
         continue;
      }

      bson_append_iter (&parts->extra, bson_iter_key (iter), -1, iter);
   }

   RETURN (true);
}


static void
_mongoc_cmd_parts_ensure_copied (mongoc_cmd_parts_t *parts)
{
   if (parts->assembled.command == parts->body) {
      bson_concat (&parts->assembled_body, parts->body);
      bson_concat (&parts->assembled_body, &parts->extra);
      parts->assembled.command = &parts->assembled_body;
   }
}


/* The server type must be mongos. */
static void
_mongoc_cmd_parts_add_read_prefs (bson_t *query,
                                  const mongoc_read_prefs_t *prefs,
                                  const mongoc_server_stream_t *server_stream)
{
   bson_t child;
   const char *mode_str;
   const bson_t *tags;
   int64_t stale;

   mode_str = _mongoc_read_mode_as_str (mongoc_read_prefs_get_mode (prefs));
   tags = mongoc_read_prefs_get_tags (prefs);
   stale = mongoc_read_prefs_get_max_staleness_seconds (prefs);

   bson_append_document_begin (query, "$readPreference", 15, &child);
   bson_append_utf8 (&child, "mode", 4, mode_str, -1);
   if (!bson_empty0 (tags)) {
      bson_append_array (&child, "tags", 4, tags);
   }

   if (stale != MONGOC_NO_MAX_STALENESS) {
      bson_append_int64 (&child, "maxStalenessSeconds", 19, stale);
   }

   bson_append_document_end (query, &child);
}


static void
_iter_concat (bson_t *dst, bson_iter_t *iter)
{
   uint32_t len;
   const uint8_t *data;
   bson_t src;

   bson_iter_document (iter, &len, &data);
   bson_init_static (&src, data, len);
   bson_concat (dst, &src);
}


/* Update result with the read prefs. Server must be mongos.
 */
static void
_mongoc_cmd_parts_assemble_mongos (mongoc_cmd_parts_t *parts,
                                   const mongoc_server_stream_t *server_stream)
{
   mongoc_read_mode_t mode;
   const bson_t *tags = NULL;
   bool add_read_prefs = false;
   bson_t query;
   bson_iter_t dollar_query;
   bool has_dollar_query = false;

   ENTRY;

   mode = mongoc_read_prefs_get_mode (parts->read_prefs);
   if (parts->read_prefs) {
      tags = mongoc_read_prefs_get_tags (parts->read_prefs);
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
   switch (mode) {
   case MONGOC_READ_PRIMARY:
      break;
   case MONGOC_READ_SECONDARY_PREFERRED:
      if (!bson_empty0 (tags)) {
         add_read_prefs = true;
      }
      parts->assembled.query_flags |= MONGOC_QUERY_SLAVE_OK;
      break;
   case MONGOC_READ_PRIMARY_PREFERRED:
   case MONGOC_READ_SECONDARY:
   case MONGOC_READ_NEAREST:
   default:
      parts->assembled.query_flags |= MONGOC_QUERY_SLAVE_OK;
      add_read_prefs = true;
   }

   if (add_read_prefs) {
      /* produce {$query: {user query}, $readPreference: ... } */
      bson_append_document_begin (&parts->assembled_body, "$query", 6, &query);

      if (bson_iter_init_find (&dollar_query, parts->body, "$query")) {
         /* user provided something like {$query: {key: "x"}} */
         has_dollar_query = true;
         _iter_concat (&query, &dollar_query);
      } else {
         bson_concat (&query, parts->body);
      }

      bson_concat (&query, &parts->extra);
      bson_append_document_end (&parts->assembled_body, &query);
      _mongoc_cmd_parts_add_read_prefs (
         &parts->assembled_body, parts->read_prefs, server_stream);

      if (has_dollar_query) {
         /* copy anything that isn't in user's $query */
         bson_copy_to_excluding_noinit (
            parts->body, &parts->assembled_body, "$query", NULL);
      }

      parts->assembled.command = &parts->assembled_body;
   } else if (bson_iter_init_find (&dollar_query, parts->body, "$query")) {
      /* user provided $query, we have no read prefs */
      bson_append_document_begin (&parts->assembled_body, "$query", 6, &query);
      _iter_concat (&query, &dollar_query);
      bson_concat (&query, &parts->extra);
      bson_append_document_end (&parts->assembled_body, &query);
      /* copy anything that isn't in user's $query */
      bson_copy_to_excluding_noinit (
         parts->body, &parts->assembled_body, "$query", NULL);

      parts->assembled.command = &parts->assembled_body;
   }

   if (!bson_empty (&parts->extra)) {
      /* if none of the above logic has merged "extra", do it now */
      _mongoc_cmd_parts_ensure_copied (parts);
   }

   EXIT;
}


static void
_mongoc_cmd_parts_assemble_mongod (mongoc_cmd_parts_t *parts,
                                   const mongoc_server_stream_t *server_stream)
{
   ENTRY;

   if (!parts->is_write_command) {
      switch (server_stream->topology_type) {
      case MONGOC_TOPOLOGY_SINGLE:
         /* Server Selection Spec: for topology type single and server types
          * besides mongos, "clients MUST always set the slaveOK wire
          * protocol flag on reads to ensure that any server type can handle
          * the request."
          */
         parts->assembled.query_flags |= MONGOC_QUERY_SLAVE_OK;
         break;

      case MONGOC_TOPOLOGY_RS_NO_PRIMARY:
      case MONGOC_TOPOLOGY_RS_WITH_PRIMARY:
         /* Server Selection Spec: for RS topology types, "For all read
          * preferences modes except primary, clients MUST set the slaveOK wire
          * protocol flag to ensure that any suitable server can handle the
          * request. Clients MUST  NOT set the slaveOK wire protocol flag if the
          * read preference mode is primary.
          */
         if (parts->read_prefs &&
             parts->read_prefs->mode != MONGOC_READ_PRIMARY) {
            parts->assembled.query_flags |= MONGOC_QUERY_SLAVE_OK;
         }

         break;
      case MONGOC_TOPOLOGY_SHARDED:
      case MONGOC_TOPOLOGY_UNKNOWN:
      case MONGOC_TOPOLOGY_DESCRIPTION_TYPES:
      default:
         /* must not call this function w/ sharded or unknown topology type */
         BSON_ASSERT (false);
      }
   } /* if (!parts->is_write_command) */

   if (!bson_empty (&parts->extra)) {
      _mongoc_cmd_parts_ensure_copied (parts);
   }

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cmd_parts_assemble --
 *
 *       Assemble the command body, options, and read preference into one
 *       command.
 *
 * Return:
 *       True if the options were successfully applied. If any options are
 *       invalid, returns false and fills out @error. In that case @parts is
 *       invalid and must not be used.
 *
 * Side effects:
 *       May partly assemble before returning an error.
 *       mongoc_cmd_parts_cleanup should be called in all cases.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_cmd_parts_assemble (mongoc_cmd_parts_t *parts,
                           const mongoc_server_stream_t *server_stream,
                           bson_error_t *error)
{
   mongoc_server_description_type_t server_type;

   ENTRY;

   BSON_ASSERT (parts);
   BSON_ASSERT (server_stream);

   server_type = server_stream->sd->type;

   /* must not be assembled already */
   BSON_ASSERT (!parts->assembled.command);
   BSON_ASSERT (bson_empty (&parts->assembled_body));

   /* begin with raw flags/cmd as assembled flags/cmd, might change below */
   parts->assembled.command = parts->body;
   parts->assembled.query_flags = parts->user_query_flags;
   parts->assembled.server_stream = server_stream;
   parts->assembled.command_name =
      _mongoc_get_command_name (parts->assembled.command);

   if (!parts->assembled.command_name) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "Empty command document");
      RETURN (false);
   }

   TRACE ("Preparing '%s'", parts->assembled.command_name);

   if (server_stream->sd->max_wire_version >= WIRE_VERSION_OP_MSG) {
      if (!bson_has_field (parts->body, "$db")) {
         BSON_APPEND_UTF8 (&parts->extra, "$db", parts->assembled.db_name);
      }

      if (parts->read_prefs &&
          !bson_has_field (parts->body, "$readPreference")) {
         bson_t child;
         const bson_t *tags = NULL;
         const char *mode_str;
         int64_t stale;

         bson_append_document_begin (
            &parts->extra, "$readPreference", 15, &child);

         mode_str = _mongoc_read_mode_as_str (
            mongoc_read_prefs_get_mode (parts->read_prefs));
         bson_append_utf8 (&child, "mode", 4, mode_str, -1);

         tags = mongoc_read_prefs_get_tags (parts->read_prefs);
         if (!bson_empty0 (tags)) {
            bson_append_array (&child, "tags", 4, tags);
         }

         stale =
            mongoc_read_prefs_get_max_staleness_seconds (parts->read_prefs);
         if (stale != MONGOC_NO_MAX_STALENESS) {
            bson_append_int64 (&child, "maxStalenessSeconds", 19, stale);
         }

         bson_append_document_end (&parts->extra, &child);
      }

      if (!bson_empty (&parts->extra)) {
         _mongoc_cmd_parts_ensure_copied (parts);
      }

      if (parts->assembled.session) {
         _mongoc_cmd_parts_ensure_copied (parts);
         bson_append_document (
            &parts->assembled_body,
            "lsid",
            4,
            mongoc_client_session_get_lsid (parts->assembled.session));
      }

      if (!bson_empty (&server_stream->cluster_time)) {
         _mongoc_cmd_parts_ensure_copied (parts);
         bson_append_document (&parts->assembled_body,
                               "$clusterTime",
                               12,
                               &server_stream->cluster_time);
      }

      RETURN (true);
   }

   if (server_type == MONGOC_SERVER_MONGOS) {
      _mongoc_cmd_parts_assemble_mongos (parts, server_stream);
      RETURN (true);
   }

   _mongoc_cmd_parts_assemble_mongod (parts, server_stream);
   RETURN (true);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cmd_parts_cleanup --
 *
 *       Free memory associated with a stack-allocated mongoc_cmd_parts_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_cmd_parts_cleanup (mongoc_cmd_parts_t *parts)
{
   bson_destroy (&parts->extra);
   bson_destroy (&parts->assembled_body);
}

bool
mongoc_cmd_is_compressable (mongoc_cmd_t *cmd)
{
   BSON_ASSERT (cmd);
   BSON_ASSERT (cmd->command_name);

   return !!strcasecmp (cmd->command_name, "ismaster") &&
          !!strcasecmp (cmd->command_name, "authenticate") &&
          !!strcasecmp (cmd->command_name, "getnonce") &&
          !!strcasecmp (cmd->command_name, "saslstart") &&
          !!strcasecmp (cmd->command_name, "saslcontinue") &&
          !!strcasecmp (cmd->command_name, "createuser") &&
          !!strcasecmp (cmd->command_name, "updateuser") &&
          !!strcasecmp (cmd->command_name, "copydb") &&
          !!strcasecmp (cmd->command_name, "copydbsaslstart") &&
          !!strcasecmp (cmd->command_name, "copydbgetnonce");
}
