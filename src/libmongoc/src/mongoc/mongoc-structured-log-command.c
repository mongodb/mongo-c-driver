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


#if defined(__linux__)
#include <sys/syscall.h>
#elif defined(_WIN32)
#include <process.h>
#elif defined(__FreeBSD__)
#include <sys/thr.h>
#else
#include <unistd.h>
#endif
#include <time.h>

#include "mongoc-structured-log.h"
#include "mongoc-structured-log-private.h"
#include "mongoc-structured-log-command-private.h"

static void
_mongoc_log_structured_append_command_data (
   mongoc_structured_log_entry_t *entry)
{
   _mongoc_structured_log_command_t *log_command = entry->command;

   BCON_APPEND (entry->structured_message,
                "commandName",
                BCON_UTF8 (log_command->command_name),
                "requestId",
                BCON_INT32 (log_command->request_id),
                "operationId",
                BCON_INT64 (log_command->operation_id),
                "driverConnectionId",
                BCON_INT32 (log_command->driver_connection_id),
                "serverConnectionId",
                BCON_INT32 (log_command->server_connection_id),
                "explicitSession",
                BCON_BOOL (log_command->explicit_session));
}

static void
mongoc_log_structured_build_command_started_message (
   mongoc_structured_log_entry_t *entry)
{
   char *cmd_json;
   _mongoc_structured_log_command_t *log_command = entry->command;

   BSON_ASSERT (entry->component == MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND);

   cmd_json = bson_as_canonical_extended_json (log_command->command, NULL);

   _mongoc_log_structured_append_command_data (entry);

   BCON_APPEND (entry->structured_message,
                "databaseName",
                BCON_UTF8 (log_command->db_name),
                "command",
                BCON_UTF8 (cmd_json));

   bson_free (cmd_json);
}

static void
mongoc_log_structured_build_command_succeeded_message (
   mongoc_structured_log_entry_t *entry)
{
   char *reply_json;
   _mongoc_structured_log_command_t *log_command = entry->command;

   BSON_ASSERT (entry->component == MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND);

   reply_json = bson_as_canonical_extended_json (log_command->reply, NULL);

   _mongoc_log_structured_append_command_data (entry);

   BCON_APPEND (entry->structured_message,
                "duration",
                BCON_INT64 (log_command->duration),
                "reply",
                BCON_UTF8 (reply_json));

   bson_free (reply_json);
}

static void
mongoc_log_structured_build_command_failed_message (
   mongoc_structured_log_entry_t *entry)
{
   char *reply_json;
   _mongoc_structured_log_command_t *log_command = entry->command;

   BSON_ASSERT (entry->component == MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND);

   reply_json = bson_as_canonical_extended_json (log_command->reply, NULL);

   _mongoc_log_structured_append_command_data (entry);

   BCON_APPEND (entry->structured_message, "reply", BCON_UTF8 (reply_json));

   if (log_command->error) {
      BCON_APPEND (entry->structured_message,
                   "failure",
                   BCON_UTF8 (log_command->error->message));
   }

   bson_free (reply_json);
}

void
mongoc_structured_log_command_started (const bson_t *command,
                                       const char *command_name,
                                       const char *db_name,
                                       int64_t operation_id,
                                       uint32_t request_id,
                                       uint32_t driver_connection_id,
                                       uint32_t server_connection_id,
                                       bool explicit_session)
{
   _mongoc_structured_log_command_t command_log = {
      .command = command,
      .command_name = command_name,
      .db_name = db_name,
      .operation_id = operation_id,
      .request_id = request_id,
      .driver_connection_id = driver_connection_id,
      .server_connection_id = server_connection_id,
      .explicit_session = explicit_session,
   };

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_INFO,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Command started",
                          mongoc_log_structured_build_command_started_message,
                          &command_log);
}

void
mongoc_structured_log_command_success (const char *command_name,
                                       int64_t operation_id,
                                       const bson_t *reply,
                                       uint64_t duration,
                                       uint32_t request_id,
                                       uint32_t driver_connection_id,
                                       uint32_t server_connection_id,
                                       bool explicit_session)
{
   _mongoc_structured_log_command_t command_log = {
      .command_name = command_name,
      .operation_id = operation_id,
      .reply = reply,
      .duration = duration,
      .request_id = request_id,
      .driver_connection_id = driver_connection_id,
      .server_connection_id = server_connection_id,
      .explicit_session = explicit_session,
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
                                       uint32_t driver_connection_id,
                                       uint32_t server_connection_id,
                                       bool explicit_session)
{
   _mongoc_structured_log_command_t command_log = {
      .command_name = command_name,
      .operation_id = operation_id,
      .reply = reply,
      .error = error,
      .request_id = request_id,
      .driver_connection_id = driver_connection_id,
      .server_connection_id = server_connection_id,
      .explicit_session = explicit_session,
   };

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_INFO,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Command succeeded",
                          mongoc_log_structured_build_command_failed_message,
                          &command_log);
}
