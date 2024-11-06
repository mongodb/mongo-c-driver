/*
 * Copyright 2009-present MongoDB, Inc.
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

#include "mongoc-structured-log.h"
#include "mongoc-structured-log-private.h"
#include "mongoc-structured-log-command-private.h"

static void
_mongoc_log_structured_append_command_data (void *structured_log_data, bson_t *structured_message /* OUT */)
{
   _mongoc_structured_log_command_t *log_command = (_mongoc_structured_log_command_t *) structured_log_data;

   BCON_APPEND (structured_message,
                "commandName",
                BCON_UTF8 (log_command->command_name),
                "requestId",
                BCON_INT32 (log_command->request_id),
                "operationId",
                BCON_INT64 (log_command->operation_id),
                "serverHostname",
                BCON_UTF8 (log_command->host->host),
                "serverResolvedIPAddress",
                BCON_UTF8 (log_command->server_resolved_ip),
                "serverPort",
                BCON_INT32 (log_command->host->port));

   /* Append client port only if it was provided */
   if (log_command->client_port) {
      BCON_APPEND (structured_message, "clientPort", BCON_INT32 (log_command->client_port));
   }

   BCON_APPEND (structured_message,
                "serverConnectionId",
                BCON_INT32 (log_command->server_connection_id),
                "explicitSession",
                BCON_BOOL (log_command->explicit_session));
}

static void
mongoc_log_structured_build_command_started_message (mongoc_structured_log_component_t component,
                                                     void *structured_log_data,
                                                     bson_t *structured_message /* OUT */)
{
   char *cmd_json;
   _mongoc_structured_log_command_t *log_command = (_mongoc_structured_log_command_t *) structured_log_data;

   BSON_ASSERT (component == MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND);

   cmd_json = mongoc_structured_log_document_to_json (log_command->command);

   _mongoc_log_structured_append_command_data (structured_log_data, structured_message);

   BCON_APPEND (structured_message, "databaseName", BCON_UTF8 (log_command->db_name), "command", BCON_UTF8 (cmd_json));

   bson_free (cmd_json);
}

static void
mongoc_log_structured_build_command_succeeded_message (mongoc_structured_log_component_t component,
                                                       void *structured_log_data,
                                                       bson_t *structured_message /* OUT */)
{
   char *reply_json;
   _mongoc_structured_log_command_t *log_command = (_mongoc_structured_log_command_t *) structured_log_data;

   BSON_ASSERT (component == MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND);

   reply_json = mongoc_structured_log_document_to_json (log_command->reply);

   _mongoc_log_structured_append_command_data (structured_log_data, structured_message);

   BCON_APPEND (structured_message, "duration", BCON_INT64 (log_command->duration), "reply", BCON_UTF8 (reply_json));

   bson_free (reply_json);
}

static void
mongoc_log_structured_build_command_failed_message (mongoc_structured_log_component_t component,
                                                    void *structured_log_data,
                                                    bson_t *structured_message /* OUT */)
{
   char *reply_json;
   _mongoc_structured_log_command_t *log_command = (_mongoc_structured_log_command_t *) structured_log_data;

   BSON_ASSERT (component == MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND);

   reply_json = mongoc_structured_log_document_to_json (log_command->reply);

   _mongoc_log_structured_append_command_data (structured_log_data, structured_message);

   BCON_APPEND (structured_message, "reply", BCON_UTF8 (reply_json));

   if (log_command->error) {
      BCON_APPEND (structured_message, "failure", BCON_UTF8 (log_command->error->message));
   }

   bson_free (reply_json);
}

void
mongoc_structured_log_command_started (const bson_t *command,
                                       const char *command_name,
                                       const char *db_name,
                                       int64_t operation_id,
                                       uint32_t request_id,
                                       const mongoc_host_list_t *host,
                                       uint32_t server_connection_id,
                                       bool explicit_session)
{
   _mongoc_structured_log_command_t command_log = {
      command_name,
      db_name,
      command,
      NULL,
      NULL,
      0,
      operation_id,
      request_id,
      host,
      NULL,
      0,
      server_connection_id,
      explicit_session,
   };

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_INFO,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Command started",
                          mongoc_log_structured_build_command_started_message,
                          &command_log);
}

void
mongoc_structured_log_command_started_with_cmd (const mongoc_cmd_t *cmd,
                                                uint32_t request_id,
                                                uint32_t server_connection_id,
                                                bool explicit_session)
{
   /* Discard const modifier, we promise we won't modify this */
   bson_t *command = (bson_t *) cmd->command;
   bool command_owned = false;

   if (cmd->payload && !cmd->payload_size) {
      command = bson_copy (cmd->command);
      command_owned = true;

      _mongoc_cmd_append_payload_as_array (cmd, command);
   }

   mongoc_structured_log_command_started (command,
                                          cmd->command_name,
                                          cmd->db_name,
                                          cmd->operation_id,
                                          request_id,
                                          &cmd->server_stream->sd->host,
                                          server_connection_id,
                                          explicit_session);

   if (command_owned) {
      bson_destroy (command);
   }
}

void
mongoc_structured_log_command_success (const char *command_name,
                                       int64_t operation_id,
                                       const bson_t *reply,
                                       uint64_t duration,
                                       uint32_t request_id,
                                       const mongoc_host_list_t *host,
                                       uint32_t server_connection_id,
                                       bool explicit_session)
{
   _mongoc_structured_log_command_t command_log = {
      command_name,
      NULL,
      NULL,
      reply,
      NULL,
      duration,
      operation_id,
      request_id,
      host,
      NULL,
      0,
      server_connection_id,
      explicit_session,
   };

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_INFO,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Command succeeded",
                          mongoc_log_structured_build_command_succeeded_message,
                          &command_log);
}

void
mongoc_structured_log_command_failure (const char *command_name,
                                       int64_t operation_id,
                                       const bson_t *reply,
                                       const bson_error_t *error,
                                       uint32_t request_id,
                                       const mongoc_host_list_t *host,
                                       uint32_t server_connection_id,
                                       bool explicit_session)
{
   _mongoc_structured_log_command_t command_log = {
      command_name,
      NULL,
      NULL,
      reply,
      error,
      0,
      operation_id,
      request_id,
      host,
      NULL,
      0,
      server_connection_id,
      explicit_session,
   };

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_INFO,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Command failed",
                          mongoc_log_structured_build_command_failed_message,
                          &command_log);
}
