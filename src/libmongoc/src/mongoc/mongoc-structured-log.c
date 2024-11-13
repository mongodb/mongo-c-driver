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
#include "common-atomic-private.h"

static void
mongoc_structured_log_default_handler (const mongoc_structured_log_entry_t *entry, void *user_data);

#define STRUCTURED_LOG_COMPONENT_TABLE_SIZE (1 + (size_t) MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION)

static const char *gStructuredLogLevelNames[] = {
   "Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Informational", "Debug", "Trace"};

static const char *gStructuredLogComponentNames[] = {"command", "topology", "serverSelection", "connection"};

static struct {
   bson_mutex_t func_mutex; // Mutex prevents func reentrancy, ensures atomic updates to (func, user_data)
   mongoc_structured_log_func_t func;
   void *user_data;
   FILE *stream; // Only used by the default handler

   int32_t max_document_length;
   int component_level_table[STRUCTURED_LOG_COMPONENT_TABLE_SIZE]; // Really mongoc_structured_log_level_t; int typed to
                                                                   // support atomic fetch
} gStructuredLog = {
   .func = mongoc_structured_log_default_handler,
};

bson_t *
mongoc_structured_log_entry_message_as_bson (const mongoc_structured_log_entry_t *entry)
{
   bson_t *bson = bson_new ();
   BSON_APPEND_UTF8 (bson, "message", entry->envelope.message);
   const mongoc_structured_log_builder_stage_t *stage = entry->builder;
   while (stage->func) {
      stage = stage->func (bson, stage);
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
   bson_mutex_lock (&gStructuredLog.func_mutex);
   mcommon_atomic_ptr_exchange ((void *) &gStructuredLog.func, (void *) log_func, mcommon_memory_order_relaxed);
   gStructuredLog.user_data = user_data;
   bson_mutex_unlock (&gStructuredLog.func_mutex);
}

void
mongoc_structured_log_get_handler (mongoc_structured_log_func_t *log_func, void **user_data)
{
   bson_mutex_lock (&gStructuredLog.func_mutex);
   *log_func = gStructuredLog.func;
   *user_data = gStructuredLog.user_data;
   bson_mutex_unlock (&gStructuredLog.func_mutex);
}

void
mongoc_structured_log_set_max_level_for_component (mongoc_structured_log_component_t component,
                                                   mongoc_structured_log_level_t level)
{
   BSON_ASSERT (level >= MONGOC_STRUCTURED_LOG_LEVEL_EMERGENCY && level <= MONGOC_STRUCTURED_LOG_LEVEL_TRACE);
   unsigned table_index = (unsigned) component;
   BSON_ASSERT (table_index < STRUCTURED_LOG_COMPONENT_TABLE_SIZE);
   mcommon_atomic_int_exchange (
      &gStructuredLog.component_level_table[table_index], level, mcommon_memory_order_relaxed);
}

void
mongoc_structured_log_set_max_level_for_all_components (mongoc_structured_log_level_t level)
{
   for (int component = 0; component < STRUCTURED_LOG_COMPONENT_TABLE_SIZE; component++) {
      mongoc_structured_log_set_max_level_for_component ((mongoc_structured_log_component_t) component, level);
   }
}

mongoc_structured_log_level_t
mongoc_structured_log_get_max_level_for_component (mongoc_structured_log_component_t component)
{
   unsigned table_index = (unsigned) component;
   BSON_ASSERT (table_index < STRUCTURED_LOG_COMPONENT_TABLE_SIZE);
   return mcommon_atomic_int_fetch (&gStructuredLog.component_level_table[table_index], mcommon_memory_order_relaxed);
}

bool
_mongoc_structured_log_should_log (const mongoc_structured_log_envelope_t *envelope)
{
   return mcommon_atomic_ptr_fetch ((void *) &gStructuredLog.func, mcommon_memory_order_relaxed) &&
          envelope->level <= mongoc_structured_log_get_max_level_for_component (envelope->component);
}

void
_mongoc_structured_log_with_entry (const mongoc_structured_log_entry_t *entry)
{
   bson_mutex_lock (&gStructuredLog.func_mutex);
   mongoc_structured_log_func_t func = gStructuredLog.func;
   if (func) {
      func (entry, gStructuredLog.user_data);
   }
   bson_mutex_unlock (&gStructuredLog.func_mutex);
}

static bool
_mongoc_structured_log_get_log_level_from_env (const char *variable, mongoc_structured_log_level_t *out)
{
   const char *level = getenv (variable);
   if (!level) {
      return false;
   }
   if (mongoc_structured_log_get_named_level (level, out)) {
      return true;
   }
   MONGOC_ERROR ("Invalid log level '%s' read from environment variable %s", level, variable);
   exit (EXIT_FAILURE);
}

const char *
mongoc_structured_log_get_level_name (mongoc_structured_log_level_t level)
{
   unsigned table_index = (unsigned) level;
   const size_t table_size = sizeof gStructuredLogLevelNames / sizeof gStructuredLogLevelNames[0];
   return table_index < table_size ? gStructuredLogLevelNames[table_index] : NULL;
}

bool
mongoc_structured_log_get_named_level (const char *name, mongoc_structured_log_level_t *out)
{
   const size_t table_size = sizeof gStructuredLogLevelNames / sizeof gStructuredLogLevelNames[0];
   for (unsigned table_index = 0; table_index < table_size; table_index++) {
      if (!strcasecmp (name, gStructuredLogLevelNames[table_index])) {
         *out = (mongoc_structured_log_level_t) table_index;
         return true;
      }
   }
   return false;
}

const char *
mongoc_structured_log_get_component_name (mongoc_structured_log_component_t component)
{
   unsigned table_index = (unsigned) component;
   const size_t table_size = sizeof gStructuredLogComponentNames / sizeof gStructuredLogComponentNames[0];
   return table_index < table_size ? gStructuredLogComponentNames[table_index] : NULL;
}

bool
mongoc_structured_log_get_named_component (const char *name, mongoc_structured_log_component_t *out)
{
   const size_t table_size = sizeof gStructuredLogComponentNames / sizeof gStructuredLogComponentNames[0];
   for (unsigned table_index = 0; table_index < table_size; table_index++) {
      if (!strcasecmp (name, gStructuredLogComponentNames[table_index])) {
         *out = (mongoc_structured_log_component_t) table_index;
         return true;
      }
   }
   return false;
}

static int32_t
_mongoc_structured_log_get_max_document_length_from_env (void)
{
   const char *variable = "MONGODB_LOG_MAX_DOCUMENT_LENGTH";
   const char *max_length_str = getenv (variable);

   if (!max_length_str) {
      return MONGOC_STRUCTURED_LOG_DEFAULT_MAX_DOCUMENT_LENGTH;
   }

   if (!strcmp (max_length_str, "unlimited")) {
      return BSON_MAX_LEN_UNLIMITED;
   }

   char *endptr;
   long int_value = strtol (max_length_str, &endptr, 10);
   if (int_value >= 0 && endptr != max_length_str && !*endptr) {
      return (int32_t) int_value;
   }

   MONGOC_ERROR ("Invalid length '%s' read from environment variable %s", max_length_str, variable);
   exit (EXIT_FAILURE);
}

void
_mongoc_structured_log_init (void)
{
   bson_mutex_init (&gStructuredLog.func_mutex);
   gStructuredLog.max_document_length = _mongoc_structured_log_get_max_document_length_from_env ();

   mongoc_structured_log_level_t level;
   if (!_mongoc_structured_log_get_log_level_from_env ("MONGODB_LOG_ALL", &level)) {
      level = MONGOC_STRUCTURED_LOG_DEFAULT_LEVEL;
   }
   mongoc_structured_log_set_max_level_for_all_components (level);

   if (_mongoc_structured_log_get_log_level_from_env ("MONGODB_LOG_COMMAND", &level)) {
      mongoc_structured_log_set_max_level_for_component (MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND, level);
   }
   if (_mongoc_structured_log_get_log_level_from_env ("MONGODB_LOG_CONNECTION", &level)) {
      mongoc_structured_log_set_max_level_for_component (MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION, level);
   }
   if (_mongoc_structured_log_get_log_level_from_env ("MONGODB_LOG_TOPOLOGY", &level)) {
      mongoc_structured_log_set_max_level_for_component (MONGOC_STRUCTURED_LOG_COMPONENT_TOPOLOGY, level);
   }
   if (_mongoc_structured_log_get_log_level_from_env ("MONGODB_LOG_SERVER_SELECTION", &level)) {
      mongoc_structured_log_set_max_level_for_component (MONGOC_STRUCTURED_LOG_COMPONENT_SERVER_SELECTION, level);
   }
}

static FILE *
_mongoc_structured_log_open_stream (void)
{
   const char *log_target = getenv ("MONGODB_LOG_PATH");
   bool log_to_stderr = !log_target || !strcmp (log_target, "stderr");
   FILE *log_stream = log_to_stderr ? stderr : fopen (log_target, "a");
   if (!log_stream) {
      MONGOC_ERROR ("Cannot open log file %s for writing", log_target);
      exit (EXIT_FAILURE);
   }
   return log_stream;
}

static FILE *
_mongoc_structured_log_get_stream (void)
{
   FILE *log_stream = gStructuredLog.stream;
   if (log_stream) {
      return log_stream;
   }
   log_stream = _mongoc_structured_log_open_stream ();
   gStructuredLog.stream = log_stream;
   return log_stream;
}

static void
mongoc_structured_log_default_handler (const mongoc_structured_log_entry_t *entry, void *user_data)
{
   bson_t *bson_message = mongoc_structured_log_entry_message_as_bson (entry);
   char *json_message = bson_as_relaxed_extended_json (bson_message, NULL);

   fprintf (_mongoc_structured_log_get_stream (),
            "MONGODB_LOG %s %s %s\n",
            mongoc_structured_log_get_level_name (mongoc_structured_log_entry_get_level (entry)),
            mongoc_structured_log_get_component_name (mongoc_structured_log_entry_get_component (entry)),
            json_message);

   bson_free (json_message);
   bson_destroy (bson_message);
}

char *
mongoc_structured_log_document_to_json (const bson_t *document, size_t *length)
{
   bson_json_opts_t *opts = bson_json_opts_new (BSON_JSON_MODE_CANONICAL, gStructuredLog.max_document_length);
   char *json = bson_as_json_with_opts (document, length, opts);
   bson_json_opts_destroy (opts);
   return json;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_utf8 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   if (key_or_null) {
      bson_append_utf8 (bson, key_or_null, -1, stage->arg2.utf8, -1);
   }
   return stage + 1;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_utf8_n_stage0 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   BSON_ASSERT (stage[1].func == _mongoc_structured_log_append_utf8_n_stage1);
   const char *key_or_null = stage[0].arg1.utf8;
   int32_t key_len = stage[0].arg2.int32;
   const char *value = stage[1].arg1.utf8;
   int32_t value_len = stage[1].arg2.int32;
   if (key_or_null) {
      bson_append_utf8 (bson, key_or_null, key_len, value, value_len);
   }
   return stage + 2;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_utf8_n_stage1 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   // Never called, marks the second stage in a two-stage utf8_n
   BSON_UNUSED (bson);
   BSON_UNUSED (stage);
   BSON_ASSERT (false);
   return NULL;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_int32 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   if (key_or_null) {
      bson_append_int32 (bson, key_or_null, -1, stage->arg2.int32);
   }
   return stage + 1;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_int64 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   if (key_or_null) {
      bson_append_int64 (bson, key_or_null, -1, stage->arg2.int64);
   }
   return stage + 1;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_boolean (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   if (key_or_null) {
      bson_append_bool (bson, key_or_null, -1, stage->arg2.boolean);
   }
   return stage + 1;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_oid_as_hex (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   if (key_or_null) {
      char str[25];
      bson_oid_to_string (stage->arg2.oid, str);
      bson_append_utf8 (bson, key_or_null, -1, str, 24);
   }
   return stage + 1;
}

const mongoc_structured_log_builder_stage_t *
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
   return stage + 1;
}

const mongoc_structured_log_builder_stage_t *
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
   return stage + 1;
}

const mongoc_structured_log_builder_stage_t *
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
      static const bson_oid_t oid_zero;
      if (!bson_oid_equal (&sd->service_id, &oid_zero)) {
         char str[25];
         bson_oid_to_string (&sd->service_id, str);
         BSON_APPEND_UTF8 (bson, "serviceId", str);
      }
   }
   return stage + 1;
}
