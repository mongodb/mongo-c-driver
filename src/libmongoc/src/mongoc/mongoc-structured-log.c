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
#include "mongoc-apm-private.h"
#include "common-atomic-private.h"
#include "common-thread-private.h"
#include "common-oid-private.h"

#define STRUCTURED_LOG_COMPONENT_TABLE_SIZE (1 + (size_t) MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION)

static BSON_ONCE_FUN (_mongoc_structured_log_init_once);

// Canonical names for log components
static const char *gStructuredLogComponentNames[] = {"command", "topology", "serverSelection", "connection"};

// Canonical names for log levels
static const char *gStructuredLogLevelNames[] = {
   "Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Informational", "Debug", "Trace"};

// Additional valid names for log levels
static const struct {
   const char *name;
   mongoc_structured_log_level_t level;
} gStructuredLogLevelAliases[] = {{.name = "off", .level = (mongoc_structured_log_level_t) 0},
                                  {.name = "warn", .level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING},
                                  {.name = "info", .level = MONGOC_STRUCTURED_LOG_LEVEL_INFO}};

// Values for gStructuredLog.state_atomic
typedef enum {
   MONGOC_STRUCTURED_LOG_STATE_UNINITIALIZED = 0, // Must _mongoc_structured_log_ensure_init
   MONGOC_STRUCTURED_LOG_STATE_INACTIVE = 1,      // Early out
   MONGOC_STRUCTURED_LOG_STATE_ACTIVE = 2,        // Maybe active, log level added to this base
} mongoc_structured_log_state_t;

static struct {
   /* Pre-computed table combining state_atomic and component_level_table.
    * Individual values are atomic, read with relaxed memory order.
    * Table rebuilds are protected by a mutex. */
   struct {
      int component_level_plus_state[STRUCTURED_LOG_COMPONENT_TABLE_SIZE];
      bson_mutex_t build_mutex;
   } combined_table;

   // Handler state, updated under exclusive lock, callable/gettable with shared lock
   struct {
      bson_shared_mutex_t mutex;
      mongoc_structured_log_func_t func;
      void *user_data;
   } handler;

   // State only used by the default handler
   struct {
      bson_mutex_t stream_mutex;
      FILE *stream;
   } default_handler;

   // Configured max document length, only modified during initialization
   int32_t max_document_length;

   /* Main storage for public per-component max log levels.
    * Atomic mongoc_structured_log_level_t, seq_cst memory order.
    * Used only for getting/setting individual component levels.
    * Not used directly by should_log, which relies on 'combined_table'. */
   int component_level_table[STRUCTURED_LOG_COMPONENT_TABLE_SIZE];

   /* Tracks whether we might be uninitialized or have no handler.
    * Set when updating 'handler', read when recomputing 'combined_table'.
    * Atomic mongoc_structured_log_state_t, seq_cst memory order. */
   int state_atomic;
} gStructuredLog;

static BSON_INLINE void
_mongoc_structured_log_ensure_init (void)
{
   static bson_once_t init_once = BSON_ONCE_INIT;
   bson_once (&init_once, &_mongoc_structured_log_init_once);
}

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

static BSON_INLINE mongoc_structured_log_level_t
_mongoc_structured_log_get_max_level_for_component (mongoc_structured_log_component_t component)
{
   unsigned table_index = (unsigned) component;
   BSON_ASSERT (table_index < STRUCTURED_LOG_COMPONENT_TABLE_SIZE);
   return mcommon_atomic_int_fetch (&gStructuredLog.component_level_table[table_index], mcommon_memory_order_seq_cst);
}

mongoc_structured_log_level_t
mongoc_structured_log_get_max_level_for_component (mongoc_structured_log_component_t component)
{
   _mongoc_structured_log_ensure_init ();
   return _mongoc_structured_log_get_max_level_for_component (component);
}

static void
_mongoc_structured_log_update_level_plus_state (void)
{
   // Pre-calculate a table of values that combine per-component level and global state.
   // Needs to be updated when the handler is set/unset or when any level setting changes.
   bson_mutex_lock (&gStructuredLog.combined_table.build_mutex);
   int state = mcommon_atomic_int_fetch (&gStructuredLog.state_atomic, mcommon_memory_order_seq_cst);
   for (unsigned table_index = 0; table_index < STRUCTURED_LOG_COMPONENT_TABLE_SIZE; table_index++) {
      mongoc_structured_log_component_t component = (mongoc_structured_log_component_t) table_index;
      mcommon_atomic_int_exchange (&gStructuredLog.combined_table.component_level_plus_state[table_index],
                                   state == MONGOC_STRUCTURED_LOG_STATE_ACTIVE
                                      ? (int) state +
                                           (int) _mongoc_structured_log_get_max_level_for_component (component)
                                      : (int) state,
                                   mcommon_memory_order_relaxed);
   }
   bson_mutex_unlock (&gStructuredLog.combined_table.build_mutex);
}

static void
_mongoc_structured_log_set_handler (mongoc_structured_log_func_t log_func, void *user_data)
{
   bson_shared_mutex_lock (&gStructuredLog.handler.mutex); // Waits for handler invocations to end
   gStructuredLog.handler.func = log_func;
   gStructuredLog.handler.user_data = user_data;
   mcommon_atomic_int_exchange (&gStructuredLog.state_atomic,
                                log_func == NULL ? MONGOC_STRUCTURED_LOG_STATE_INACTIVE
                                                 : MONGOC_STRUCTURED_LOG_STATE_ACTIVE,
                                mcommon_memory_order_seq_cst);
   bson_shared_mutex_unlock (&gStructuredLog.handler.mutex);
}

void
mongoc_structured_log_set_handler (mongoc_structured_log_func_t log_func, void *user_data)
{
   _mongoc_structured_log_ensure_init ();
   _mongoc_structured_log_set_handler (log_func, user_data);
   _mongoc_structured_log_update_level_plus_state ();
}

void
mongoc_structured_log_get_handler (mongoc_structured_log_func_t *log_func, void **user_data)
{
   _mongoc_structured_log_ensure_init ();
   bson_shared_mutex_lock_shared (&gStructuredLog.handler.mutex);
   *log_func = gStructuredLog.handler.func;
   *user_data = gStructuredLog.handler.user_data;
   bson_shared_mutex_unlock_shared (&gStructuredLog.handler.mutex);
}

static void
_mongoc_structured_log_set_max_level_for_component (mongoc_structured_log_component_t component,
                                                    mongoc_structured_log_level_t level)
{
   BSON_ASSERT (level >= MONGOC_STRUCTURED_LOG_LEVEL_EMERGENCY && level <= MONGOC_STRUCTURED_LOG_LEVEL_TRACE);
   unsigned table_index = (unsigned) component;
   BSON_ASSERT (table_index < STRUCTURED_LOG_COMPONENT_TABLE_SIZE);
   mcommon_atomic_int_exchange (
      &gStructuredLog.component_level_table[table_index], level, mcommon_memory_order_seq_cst);
}

void
mongoc_structured_log_set_max_level_for_component (mongoc_structured_log_component_t component,
                                                   mongoc_structured_log_level_t level)
{
   _mongoc_structured_log_ensure_init ();
   _mongoc_structured_log_set_max_level_for_component (component, level);
   _mongoc_structured_log_update_level_plus_state ();
}

static void
_mongoc_structured_log_set_max_level_for_all_components (mongoc_structured_log_level_t level)
{
   for (int component = 0; component < STRUCTURED_LOG_COMPONENT_TABLE_SIZE; component++) {
      _mongoc_structured_log_set_max_level_for_component ((mongoc_structured_log_component_t) component, level);
   }
}

void
mongoc_structured_log_set_max_level_for_all_components (mongoc_structured_log_level_t level)
{
   _mongoc_structured_log_ensure_init ();
   _mongoc_structured_log_set_max_level_for_all_components (level);
   _mongoc_structured_log_update_level_plus_state ();
}

bool
_mongoc_structured_log_should_log (const mongoc_structured_log_envelope_t *envelope)
{
   unsigned table_index = (unsigned) envelope->component;
   BSON_ASSERT (table_index < STRUCTURED_LOG_COMPONENT_TABLE_SIZE);
   const int volatile *level_plus_state_ptr = &gStructuredLog.combined_table.component_level_plus_state[table_index];

   // One atomic fetch gives us the per-component level, the global
   // enable/disable state, and lets us detect whether initialization is needed.
   int level_plus_state = mcommon_atomic_int_fetch (level_plus_state_ptr, mcommon_memory_order_relaxed);
   if (BSON_UNLIKELY (level_plus_state == (int) MONGOC_STRUCTURED_LOG_STATE_UNINITIALIZED)) {
      _mongoc_structured_log_ensure_init ();
      level_plus_state = mcommon_atomic_int_fetch (level_plus_state_ptr, mcommon_memory_order_relaxed);
   }
   return envelope->level + MONGOC_STRUCTURED_LOG_STATE_ACTIVE <= level_plus_state;
}

void
_mongoc_structured_log_with_entry (const mongoc_structured_log_entry_t *entry)
{
   // should_log has guaranteed that we've run init by now.
   // No guarantee that there's actually a handler set but it's likely.
   bson_shared_mutex_lock_shared (&gStructuredLog.handler.mutex);
   mongoc_structured_log_func_t func = gStructuredLog.handler.func;
   if (BSON_LIKELY (func)) {
      func (entry, gStructuredLog.handler.user_data);
   }
   bson_shared_mutex_unlock_shared (&gStructuredLog.handler.mutex);
}

static bool
_mongoc_structured_log_get_log_level_from_env (const char *variable,
                                               mongoc_structured_log_level_t *out,
                                               int volatile *err_count_atomic)
{
   const char *level = getenv (variable);
   if (!level) {
      return false;
   }
   if (mongoc_structured_log_get_named_level (level, out)) {
      return true;
   }
   // Only report the first instance of each error
   if (0 == mcommon_atomic_int_fetch_add (err_count_atomic, 1, mcommon_memory_order_seq_cst)) {
      MONGOC_WARNING ("Invalid log level '%s' read from environment variable %s. Ignoring it.", level, variable);
   }
   return false;
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
   // First check canonical names
   {
      const size_t table_size = sizeof gStructuredLogLevelNames / sizeof gStructuredLogLevelNames[0];
      for (unsigned table_index = 0; table_index < table_size; table_index++) {
         if (!strcasecmp (name, gStructuredLogLevelNames[table_index])) {
            *out = (mongoc_structured_log_level_t) table_index;
            return true;
         }
      }
   }
   // Check additional acceptable names
   {
      const size_t table_size = sizeof gStructuredLogLevelAliases / sizeof gStructuredLogLevelAliases[0];
      for (unsigned table_index = 0; table_index < table_size; table_index++) {
         const char *alias = gStructuredLogLevelAliases[table_index].name;
         mongoc_structured_log_level_t level = gStructuredLogLevelAliases[table_index].level;
         if (!strcasecmp (name, alias)) {
            *out = level;
            return true;
         }
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

   MONGOC_WARNING ("Invalid length '%s' read from environment variable %s. Ignoring it.", max_length_str, variable);
   return MONGOC_STRUCTURED_LOG_DEFAULT_MAX_DOCUMENT_LENGTH;
}

static void
_mongoc_structured_log_set_max_levels_from_env (void)
{
   mongoc_structured_log_level_t level;
   {
      static int err_count_atomic = 0;
      if (_mongoc_structured_log_get_log_level_from_env ("MONGODB_LOG_ALL", &level, &err_count_atomic)) {
         _mongoc_structured_log_set_max_level_for_all_components (level);
      }
   }
   {
      static int err_count_atomic = 0;
      if (_mongoc_structured_log_get_log_level_from_env ("MONGODB_LOG_COMMAND", &level, &err_count_atomic)) {
         _mongoc_structured_log_set_max_level_for_component (MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND, level);
      }
   }
   {
      static int err_count_atomic = 0;
      if (_mongoc_structured_log_get_log_level_from_env ("MONGODB_LOG_CONNECTION", &level, &err_count_atomic)) {
         _mongoc_structured_log_set_max_level_for_component (MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION, level);
      }
   }
   {
      static int err_count_atomic = 0;
      if (_mongoc_structured_log_get_log_level_from_env ("MONGODB_LOG_TOPOLOGY", &level, &err_count_atomic)) {
         _mongoc_structured_log_set_max_level_for_component (MONGOC_STRUCTURED_LOG_COMPONENT_TOPOLOGY, level);
      }
   }
   {
      static int err_count_atomic = 0;
      if (_mongoc_structured_log_get_log_level_from_env ("MONGODB_LOG_SERVER_SELECTION", &level, &err_count_atomic)) {
         _mongoc_structured_log_set_max_level_for_component (MONGOC_STRUCTURED_LOG_COMPONENT_SERVER_SELECTION, level);
      }
   }
}

void
mongoc_structured_log_set_max_levels_from_env (void)
{
   _mongoc_structured_log_ensure_init ();
   _mongoc_structured_log_set_max_levels_from_env ();
   _mongoc_structured_log_update_level_plus_state ();
}

static FILE *
_mongoc_structured_log_open_stream (void)
{
   const char *path = getenv ("MONGODB_LOG_PATH");
   if (!path || !strcmp (path, "stderr")) {
      return stderr;
   }
   if (!strcmp (path, "stdout")) {
      return stdout;
   }
   FILE *file = fopen (path, "a");
   if (!file) {
      MONGOC_WARNING ("Cannot open log file '%s' for writing. Logging to stderr instead.", path);
      return stderr;
   }
   return file;
}

static FILE *
_mongoc_structured_log_get_stream (void)
{
   // Not re-entrant; protected by the default_handler.stream_mutex.
   FILE *log_stream = gStructuredLog.default_handler.stream;
   if (log_stream) {
      return log_stream;
   }
   // Note that log_stream may be the global stderr/stdout streams,
   // or an allocated FILE that is never closed.
   log_stream = _mongoc_structured_log_open_stream ();
   gStructuredLog.default_handler.stream = log_stream;
   return log_stream;
}

static void
mongoc_structured_log_default_handler (const mongoc_structured_log_entry_t *entry, void *user_data)
{
   // We can serialize the message before taking the stream lock
   bson_t *bson_message = mongoc_structured_log_entry_message_as_bson (entry);
   char *json_message = bson_as_relaxed_extended_json (bson_message, NULL);
   bson_destroy (bson_message);

   const char *level_name = mongoc_structured_log_get_level_name (mongoc_structured_log_entry_get_level (entry));
   const char *component_name =
      mongoc_structured_log_get_component_name (mongoc_structured_log_entry_get_component (entry));

   bson_mutex_lock (&gStructuredLog.default_handler.stream_mutex);
   fprintf (_mongoc_structured_log_get_stream (), "MONGODB_LOG %s %s %s\n", level_name, component_name, json_message);
   bson_mutex_unlock (&gStructuredLog.default_handler.stream_mutex);

   bson_free (json_message);
}

static BSON_ONCE_FUN (_mongoc_structured_log_init_once)
{
   bson_shared_mutex_init (&gStructuredLog.handler.mutex);
   bson_mutex_init (&gStructuredLog.default_handler.stream_mutex);
   bson_mutex_init (&gStructuredLog.combined_table.build_mutex);

   gStructuredLog.max_document_length = _mongoc_structured_log_get_max_document_length_from_env ();

   _mongoc_structured_log_set_max_level_for_all_components (MONGOC_STRUCTURED_LOG_DEFAULT_LEVEL);
   _mongoc_structured_log_set_max_levels_from_env ();

   // The default handler replaces MONGOC_STRUCTURED_LOG_STATE_MAYBE_UNINITIALIZED, this must be last.
   _mongoc_structured_log_set_handler (mongoc_structured_log_default_handler, NULL);
   _mongoc_structured_log_update_level_plus_state ();

   BSON_ONCE_RETURN;
}

char *
mongoc_structured_log_document_to_json (const bson_t *document, size_t *length)
{
   bson_json_opts_t *opts = bson_json_opts_new (BSON_JSON_MODE_RELAXED, gStructuredLog.max_document_length);
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
   const bson_oid_t *oid_or_null = stage->arg2.oid;
   if (key_or_null) {
      if (oid_or_null) {
         char str[25];
         bson_oid_to_string (oid_or_null, str);
         bson_append_utf8 (bson, key_or_null, -1, str, 24);
      } else {
         bson_append_null (bson, key_or_null, -1);
      }
   }
   return stage + 1;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_bson_as_json (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *key_or_null = stage->arg1.utf8;
   const bson_t *bson_or_null = stage->arg2.bson;
   if (key_or_null) {
      if (bson_or_null) {
         size_t json_length;
         char *json = mongoc_structured_log_document_to_json (bson_or_null, &json_length);
         if (json) {
            bson_append_utf8 (bson, key_or_null, -1, json, json_length);
            bson_free (json);
         }
      } else {
         bson_append_null (bson, key_or_null, -1);
      }
   }
   return stage + 1;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const mongoc_cmd_t *cmd = stage->arg1.cmd;
   const mongoc_structured_log_cmd_flags_t flags = stage->arg2.cmd_flags;

   BSON_ASSERT (cmd);

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
      if (mongoc_apm_is_sensitive_command_message (cmd->command_name, cmd->command)) {
         BSON_APPEND_UTF8 (bson, "command", "{}");
      } else {
         bson_t *command_copy = NULL;

         if (cmd->payloads_count > 0) {
            // @todo This is a performance bottleneck, we shouldn't be copying
            //       a potentially large command to serialize a potentially very
            //       small part of it. We should be appending JSON to a single buffer
            //       for all nesting levels, constrained by length limit, while visiting
            //       borrowed references to each command attribute and each payload. CDRIVER-4814
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
   return stage + 1;
}

static void
_mongoc_structured_log_append_redacted_cmd_reply (bson_t *bson, bool is_sensitive, const bson_t *reply)
{
   if (is_sensitive) {
      BSON_APPEND_UTF8 (bson, "reply", "{}");
   } else {
      size_t json_length;
      char *json = mongoc_structured_log_document_to_json (reply, &json_length);
      if (json) {
         const char *key = "reply";
         bson_append_utf8 (bson, key, strlen (key), json, json_length);
         bson_free (json);
      }
   }
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_reply (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const mongoc_cmd_t *cmd = stage->arg1.cmd;
   const bson_t *reply = stage->arg2.bson;

   BSON_ASSERT (cmd);
   BSON_ASSERT (reply);

   bool is_sensitive = mongoc_apm_is_sensitive_command_message (cmd->command_name, cmd->command) ||
                       mongoc_apm_is_sensitive_command_message (cmd->command_name, reply);
   _mongoc_structured_log_append_redacted_cmd_reply (bson, is_sensitive, reply);
   return stage + 1;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_name_reply (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const char *cmd_name = stage->arg1.utf8;
   const bson_t *reply = stage->arg2.bson;

   BSON_ASSERT (cmd_name);
   BSON_ASSERT (reply);

   bool is_sensitive = mongoc_apm_is_sensitive_command_message (cmd_name, reply);
   _mongoc_structured_log_append_redacted_cmd_reply (bson, is_sensitive, reply);
   return stage + 1;
}

static void
_mongoc_structured_log_append_redacted_cmd_failure (bson_t *bson,
                                                    bool is_sensitive,
                                                    const bson_t *reply,
                                                    const bson_error_t *error)
{
   bool is_server_side = error->domain == MONGOC_ERROR_SERVER || error->domain == MONGOC_ERROR_WRITE_CONCERN_ERROR;
   if (is_server_side) {
      if (is_sensitive) {
         // Redacted server-side message, must be a document with at most 'code', 'codeName', 'errorLabels'
         bson_t failure;
         bson_iter_t iter;
         BSON_ASSERT (BSON_APPEND_DOCUMENT_BEGIN (bson, "failure", &failure));
         bson_iter_init (&iter, reply);
         while (bson_iter_next (&iter)) {
            const char *key = bson_iter_key (&iter);
            if (!strcmp (key, "code") || !strcmp (key, "codeName") || !strcmp (key, "errorLabels")) {
               bson_append_iter (&failure, key, bson_iter_key_len (&iter), &iter);
            }
         }
         BSON_ASSERT (bson_append_document_end (bson, &failure));
      } else {
         // Non-redacted server side message, pass through
         BSON_APPEND_DOCUMENT (bson, "failure", reply);
      }
   } else {
      // Client-side errors converted directly from bson_error_t, never redacted
      bson_t failure;
      BSON_ASSERT (BSON_APPEND_DOCUMENT_BEGIN (bson, "failure", &failure));
      BSON_APPEND_INT32 (&failure, "code", error->code);
      BSON_APPEND_INT32 (&failure, "domain", error->domain);
      BSON_APPEND_UTF8 (&failure, "message", error->message);
      BSON_ASSERT (bson_append_document_end (bson, &failure));
   }
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_failure_stage0 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   BSON_ASSERT (stage[1].func == _mongoc_structured_log_append_cmd_failure_stage1);
   const mongoc_cmd_t *cmd = stage[0].arg1.cmd;
   const bson_t *reply = stage[0].arg2.bson;
   const bson_error_t *error = stage[1].arg1.error;

   BSON_ASSERT (cmd);
   BSON_ASSERT (reply);
   BSON_ASSERT (error);

   bool is_sensitive = mongoc_apm_is_sensitive_command_message (cmd->command_name, cmd->command) ||
                       mongoc_apm_is_sensitive_command_message (cmd->command_name, reply);
   _mongoc_structured_log_append_redacted_cmd_failure (bson, is_sensitive, reply, error);
   return stage + 2;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_failure_stage1 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   // Never called, marks the second stage in a two-stage cmd_failure
   BSON_UNUSED (bson);
   BSON_UNUSED (stage);
   BSON_ASSERT (false);
   return NULL;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_name_failure_stage0 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   BSON_ASSERT (stage[1].func == _mongoc_structured_log_append_cmd_name_failure_stage1);
   const char *cmd_name = stage[0].arg1.utf8;
   const bson_t *reply = stage[0].arg2.bson;
   const bson_error_t *error = stage[1].arg1.error;

   BSON_ASSERT (cmd_name);
   BSON_ASSERT (reply);
   BSON_ASSERT (error);

   bool is_sensitive = mongoc_apm_is_sensitive_command_message (cmd_name, reply);
   _mongoc_structured_log_append_redacted_cmd_failure (bson, is_sensitive, reply, error);
   return stage + 2;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_name_failure_stage1 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   // Never called, marks the second stage in a two-stage cmd_name_failure
   BSON_UNUSED (bson);
   BSON_UNUSED (stage);
   BSON_ASSERT (false);
   return NULL;
}

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_server_description (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage)
{
   const mongoc_server_description_t *sd = stage->arg1.server_description;
   const mongoc_structured_log_server_description_flags_t flags = stage->arg2.server_description_flags;

   BSON_ASSERT (sd);

   if (flags & MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_HOST) {
      BSON_APPEND_UTF8 (bson, "serverHost", sd->host.host);
   }
   if (flags & MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_PORT) {
      BSON_APPEND_INT32 (bson, "serverPort", sd->host.port);
   }
   if (flags & MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_CONNECTION_ID) {
      int64_t server_connection_id = sd->server_connection_id;
      if (MONGOC_NO_SERVER_CONNECTION_ID != server_connection_id) {
         BSON_APPEND_INT64 (bson, "serverConnectionId", server_connection_id);
      }
   }
   if (flags & MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVICE_ID) {
      if (!mcommon_oid_is_zero (&sd->service_id)) {
         char str[25];
         bson_oid_to_string (&sd->service_id, str);
         BSON_APPEND_UTF8 (bson, "serviceId", str);
      }
   }
   return stage + 1;
}
