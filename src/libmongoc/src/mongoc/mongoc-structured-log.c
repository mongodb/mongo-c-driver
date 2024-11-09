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
#include "mongoc-thread-private.h"
#include "mongoc-util-private.h"

static void
mongoc_structured_log_default_handler (const mongoc_structured_log_entry_t *entry, void *user_data);

static bson_once_t once = BSON_ONCE_INIT;
static bson_mutex_t gStructuredLogMutex;
static mongoc_structured_log_func_t gStructuredLogger = mongoc_structured_log_default_handler;
static void *gStructuredLoggerData;
static FILE *log_stream;

static BSON_ONCE_FUN (_mongoc_ensure_mutex_once)
{
   bson_mutex_init (&gStructuredLogMutex);

   BSON_ONCE_RETURN;
}

bson_t *
mongoc_structured_log_entry_message_as_bson (const mongoc_structured_log_entry_t *entry)
{
   bson_t *bson = bson_new ();
   BSON_APPEND_UTF8 (bson, "message", entry->envelope.message);
   for (const mongoc_structured_log_builder_stage_t *stage = entry->builder; stage->func; stage++) {
      stage->func (bson, stage);
   }
   return bson;
}

mongoc_structured_log_level_t
mongoc_structured_log_entry_get_level (const mongoc_structured_log_entry_t *entry)
{
   return entry->envelope.level;
}

mongoc_structured_log_component_t
mongoc_structured_log_entry_get_component (const mongoc_structured_log_entry_t *entry)
{
   return entry->envelope.component;
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

bool
_mongoc_structured_log_should_log (const mongoc_structured_log_envelope_t *envelope)
{
   // @todo Implement early-out settings for limiting max log level
   (void) envelope;
   // Don't take mutex, no need for atomicity.
   // This should be a low cost early-out when logging is disabled.
   return gStructuredLogger != NULL;
}

void
_mongoc_structured_log_with_entry (const mongoc_structured_log_entry_t *entry)
{
   bson_once (&once, &_mongoc_ensure_mutex_once);
   bson_mutex_lock (&gStructuredLogMutex);

   if (!gStructuredLogger) {
      bson_mutex_unlock (&gStructuredLogMutex);
      return;
   }

   gStructuredLogger (entry, gStructuredLoggerData);
   bson_mutex_unlock (&gStructuredLogMutex);
}

static mongoc_structured_log_level_t
_mongoc_structured_log_get_log_level_from_env (const char *variable)
{
   const char *level = getenv (variable);

   if (!level) {
      return MONGOC_STRUCTURED_LOG_DEFAULT_LEVEL;
   } else if (!strcasecmp (level, "trace")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_TRACE;
   } else if (!strcasecmp (level, "debug")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_DEBUG;
   } else if (!strcasecmp (level, "info")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_INFO;
   } else if (!strcasecmp (level, "notice")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_NOTICE;
   } else if (!strcasecmp (level, "warn")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_WARNING;
   } else if (!strcasecmp (level, "error")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_ERROR;
   } else if (!strcasecmp (level, "critical")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_CRITICAL;
   } else if (!strcasecmp (level, "alert")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_ALERT;
   } else if (!strcasecmp (level, "emergency")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_EMERGENCY;
   } else {
      MONGOC_ERROR ("Invalid log level %s read for variable %s", level, variable);
      exit (EXIT_FAILURE);
   }
}

static mongoc_structured_log_level_t
_mongoc_structured_log_get_log_level (mongoc_structured_log_component_t component)
{
   switch (component) {
   case MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND:
      return _mongoc_structured_log_get_log_level_from_env ("MONGODB_LOGGING_COMMAND");
   case MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION:
      return _mongoc_structured_log_get_log_level_from_env ("MONGODB_LOGGING_CONNECTION");
   case MONGOC_STRUCTURED_LOG_COMPONENT_SDAM:
      return _mongoc_structured_log_get_log_level_from_env ("MONGODB_LOGGING_SDAM");
   case MONGOC_STRUCTURED_LOG_COMPONENT_SERVER_SELECTION:
      return _mongoc_structured_log_get_log_level_from_env ("MONGODB_LOGGING_SERVER_SELECTION");
   default:
      MONGOC_ERROR ("Requesting log level for unsupported component %d", (int) component);
      exit (EXIT_FAILURE);
   }
}

static void
_mongoc_structured_log_initialize_stream (void)
{
   const char *log_target = getenv ("MONGODB_LOGGING_PATH");
   bool log_to_stderr = !log_target || !strcmp (log_target, "stderr");

   log_stream = log_to_stderr ? stderr : fopen (log_target, "a");
   if (!log_stream) {
      MONGOC_ERROR ("Cannot open log file %s for writing", log_target);
      exit (EXIT_FAILURE);
   }
}

static FILE *
_mongoc_structured_log_get_stream (void)
{
   if (!log_stream) {
      _mongoc_structured_log_initialize_stream ();
   }

   return log_stream;
}

static void
mongoc_structured_log_default_handler (const mongoc_structured_log_entry_t *entry, void *user_data)
{
   // @todo This really needs a cache, we shouldn't be parsing env vars for each should_log check
   mongoc_structured_log_level_t log_level =
      _mongoc_structured_log_get_log_level (mongoc_structured_log_entry_get_component (entry));

   if (log_level < mongoc_structured_log_entry_get_level (entry)) {
      return;
   }

   bson_t *bson_message = mongoc_structured_log_entry_message_as_bson (entry);
   char *json_message = bson_as_relaxed_extended_json (bson_message, NULL);

   fprintf (_mongoc_structured_log_get_stream (),
            "Structured log: %d, %d, %s\n",
            (int) mongoc_structured_log_entry_get_level (entry),
            (int) mongoc_structured_log_entry_get_component (entry),
            json_message);

   bson_free (json_message);
   bson_destroy (bson_message);
}

static int32_t
mongoc_structured_log_get_max_length (void)
{
   const char *max_length_str = getenv ("MONGODB_LOGGING_MAX_DOCUMENT_LENGTH");

   if (!max_length_str) {
      return MONGOC_STRUCTURED_LOG_DEFAULT_MAX_DOCUMENT_LENGTH;
   }

   if (!strcmp (max_length_str, "unlimited")) {
      return BSON_MAX_LEN_UNLIMITED;
   }

   return strtoul (max_length_str, NULL, 10);
}

char *
mongoc_structured_log_document_to_json (const bson_t *document, size_t *length)
{
   bson_json_opts_t *opts = bson_json_opts_new (BSON_JSON_MODE_CANONICAL, mongoc_structured_log_get_max_length ());
   char *json = bson_as_json_with_opts (document, length, opts);
   bson_json_opts_destroy (opts);
   return json;
}

void
mongoc_structured_log_get_handler (mongoc_structured_log_func_t *log_func, void **user_data)
{
   *log_func = gStructuredLogger;
   *user_data = gStructuredLoggerData;
}

void
_mongoc_structured_log_append_utf8 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   if (key_or_null) {
      bson_append_utf8 (bson, key_or_null, -1, stage->arg2.utf8, -1);
   }
}

void
_mongoc_structured_log_append_int32 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   if (key_or_null) {
      bson_append_int32 (bson, key_or_null, -1, stage->arg2.int32);
   }
}

void
_mongoc_structured_log_append_int64 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   if (key_or_null) {
      bson_append_int64 (bson, key_or_null, -1, stage->arg2.int64);
   }
}

void
_mongoc_structured_log_append_bool (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   if (key_or_null) {
      bson_append_bool (bson, key_or_null, -1, stage->arg2.boolean);
   }
}

void
_mongoc_structured_log_append_oid_as_hex (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   if (key_or_null) {
      char str[25];
      bson_oid_to_string (stage->arg2.oid, str);
      bson_append_utf8 (bson, key_or_null, -1, str, 24);
   }
}

void
_mongoc_structured_log_append_bson_as_json (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   if (key_or_null) {
      size_t json_length;
      char *json = mongoc_structured_log_document_to_json (stage->arg2.bson, &json_length);
      if (json) {
         bson_append_utf8 (bson, key_or_null, -1, json, json_length);
         bson_free (json);
      }
   }
}

void
_mongoc_structured_log_append_cmd (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const mongoc_cmd_t *cmd = stage->arg1.cmd;
   const mongoc_structured_log_cmd_flags_t flags = stage->arg2.cmd_flags;

   if (flags & MONGOC_STRUCTURED_LOG_CMD_DATABASE_NAME) {
      BSON_APPEND_UTF8 (bson, "databaseName", cmd->db_name);
   }
   if (flags & MONGOC_STRUCTURED_LOG_CMD_COMMAND_NAME) {
      BSON_APPEND_UTF8 (bson, "commandName", cmd->command_name);
   }
   if (flags & MONGOC_STRUCTURED_LOG_CMD_OPERATION_ID) {
      BSON_APPEND_INT64 (bson, "operationId", cmd->operation_id);
   }
   if (flags & MONGOC_STRUCTURED_LOG_CMD_COMMAND) {
      bson_t *command_copy = NULL;

      // @todo This is a performance bottleneck, we shouldn't be copying
      //       a potentially large command to serialize a potentially very
      //       small part of it. We should be outputting JSON, constrained
      //       by length limit, while visiting borrowed references to each
      //       command attribute and each payload. CDRIVER-4814
      if (cmd->payloads_count > 0) {
         command_copy = bson_copy (cmd->command);
         _mongoc_cmd_append_payload_as_array (cmd, command_copy);
      }

      size_t json_length;
      char *json = mongoc_structured_log_document_to_json (command_copy ? command_copy : cmd->command, &json_length);
      if (json) {
         const char *key = "command";
         bson_append_utf8 (bson, key, strlen (key), json, json_length);
         bson_free (json);
      }

      bson_destroy (command_copy);
   }
}

void
_mongoc_structured_log_append_server_description (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const mongoc_server_description_t *sd = stage->arg1.server_description;
   const mongoc_structured_log_server_description_flags_t flags = stage->arg2.server_description_flags;

   if (flags & MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_HOST) {
      BSON_APPEND_UTF8 (bson, "serverHost", sd->host.host);
   }
   if (flags & MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_PORT) {
      BSON_APPEND_INT32 (bson, "serverPort", sd->host.port);
   }
   if (flags & MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_CONNECTION_ID) {
      BSON_APPEND_INT64 (bson, "serverConnectionId", sd->server_connection_id);
   }
   if (flags & MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVICE_ID) {
      char str[25];
      bson_oid_to_string (&sd->service_id, str);
      BSON_APPEND_UTF8 (bson, "serviceId", str);
   }
}
