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
#include <stdarg.h>
#include <time.h>

#include "mongoc-structured-log.h"
#include "mongoc-structured-log-private.h"
#include "mongoc-thread-private.h"

static bson_once_t once = BSON_ONCE_INIT;
static bson_mutex_t gStructuredLogMutex;
static mongoc_structured_log_func_t gStructuredLogger = mongoc_structured_log_default_handler;
static void *gStructuredLoggerData;

static BSON_ONCE_FUN (_mongoc_ensure_mutex_once)
{
   bson_mutex_init (&gStructuredLogMutex);

   BSON_ONCE_RETURN;
}

static void
mongoc_structured_log_entry_init (mongoc_structured_log_entry_t *entry,
                                  mongoc_structured_log_level_t level,
                                  mongoc_structured_log_component_t component,
                                  const char *message,
                                  mongoc_structured_log_build_context_t build_context,
                                  va_list *context_data)
{
   entry->level = level;
   entry->component = component;
   entry->message = message;
   entry->build_context = build_context;
   entry->context_data = context_data;
   entry->context = NULL;
}

static void
mongoc_structured_log_entry_destroy (mongoc_structured_log_entry_t *entry)
{
   if (entry->context) {
      bson_free (entry->context);
   }
}

bson_t*
mongoc_structured_log_entry_get_context (mongoc_structured_log_entry_t *entry)
{
   if (entry->context) {
      return entry->context;
   }

   entry->context = BCON_NEW(
      "message",
      BCON_UTF8(entry->message)
   );

   if (entry->build_context) {
      entry->build_context(entry->context, entry->context_data);
   }

   return entry->context;
}

mongoc_structured_log_level_t
mongoc_structured_log_entry_get_level (mongoc_structured_log_entry_t *entry)
{
   return entry->level;
}

mongoc_structured_log_component_t
mongoc_structured_log_entry_get_component (mongoc_structured_log_entry_t *entry)
{
   return entry->component;
}

void
mongoc_structured_log_set_handler (mongoc_structured_log_func_t log_func, void *user_data)
{
   bson_once (&once, &_mongoc_ensure_mutex_once);

   bson_mutex_lock (&gStructuredLogMutex);
   gStructuredLogger = log_func;
   gStructuredLoggerData = user_data;
   bson_mutex_unlock (&gStructuredLogMutex);
}

void
mongoc_structured_log (mongoc_structured_log_level_t level,
                       mongoc_structured_log_component_t component,
                       const char *message,
                       mongoc_structured_log_build_context_t build_context,
                       ...)
{
   va_list context_data;
   mongoc_structured_log_entry_t entry;

   if (!gStructuredLogger) {
      return;
   }

   va_start (context_data, build_context);
   mongoc_structured_log_entry_init (&entry, level, component, message, build_context, &context_data);

   bson_mutex_lock (&gStructuredLogMutex);
   gStructuredLogger (&entry, gStructuredLoggerData);
   bson_mutex_unlock (&gStructuredLogMutex);

   mongoc_structured_log_entry_destroy (&entry);
   va_end (context_data);
}

static void
mongoc_log_structured_build_command_context(bson_t *context, va_list *context_data)
{
   mongoc_cmd_t *cmd = va_arg (*context_data, mongoc_cmd_t*);
   uint32_t request_id = va_arg (*context_data, uint32_t);
   uint32_t driver_connection_id = va_arg (*context_data, uint32_t);
   uint32_t server_connection_id = va_arg (*context_data, uint32_t);
   bool explicit_session = !!va_arg (*context_data, int);

   char* cmd_json = bson_as_canonical_extended_json(cmd->command, NULL);

   BCON_APPEND(
      context,
      "command",
      BCON_UTF8(cmd_json),
      "databaseName",
      BCON_UTF8(cmd->db_name),
      "commandName",
      BCON_UTF8(cmd->command_name),
      "requestId",
      BCON_INT32 (request_id),
      "operationId",
      BCON_INT64(cmd->operation_id),
      "driverConnectionId",
      BCON_INT32 (driver_connection_id),
      "serverConnectionId",
      BCON_INT32 (server_connection_id),
      "explicitSession",
      BCON_BOOL(explicit_session)
   );

   bson_free (cmd_json);
}

void
mongoc_structured_log_command_started (mongoc_cmd_t *cmd,
                                       uint32_t request_id,
                                       uint32_t driver_connection_id,
                                       uint32_t server_connection_id,
                                       bool explicit_session)
{
   mongoc_structured_log (
      MONGOC_STRUCTURED_LOG_LEVEL_INFO,
      MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      "Command started",
      mongoc_log_structured_build_command_context,
      cmd,
      request_id,
      driver_connection_id,
      server_connection_id,
      explicit_session
   );
}

void
mongoc_structured_log_default_handler (mongoc_structured_log_entry_t *entry, void *user_data)
{
   char *message = bson_as_json (mongoc_structured_log_entry_get_context (entry), NULL);

   fprintf (stderr,
            "Structured log: %d, %d, %s",
            mongoc_structured_log_entry_get_level (entry),
            mongoc_structured_log_entry_get_component (entry),
            message);

   bson_free (message);
}
