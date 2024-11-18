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

#include "mongoc-prelude.h"

#ifndef MONGOC_STRUCTURED_LOG_PRIVATE_H
#define MONGOC_STRUCTURED_LOG_PRIVATE_H

#include <bson/bson.h>
#include <common-bson-dsl-private.h>
#include "mongoc-structured-log.h"
#include "mongoc-cmd-private.h"
#include "mongoc-server-description-private.h"

BSON_BEGIN_DECLS

#define MONGOC_STRUCTURED_LOG_DEFAULT_LEVEL MONGOC_STRUCTURED_LOG_LEVEL_WARNING
#define MONGOC_STRUCTURED_LOG_DEFAULT_MAX_DOCUMENT_LENGTH 1000

/**
 * @def mognoc_structured_log(level, component, message, ...)
 * @brief Write to the libmongoc structured log.
 *
 * @param level Log level, as a mongoc_structured_log_level_t expression
 * @param component Log component, as a mongoc_structured_log_component_t expression
 * @param message Log message, as a const char* expression
 * @param ... Optional list of log 'items' that specify additional information to include.
 *
 * The level, component, and message expressions are always evaluated.
 * Any expressions in the optional items list are only evaluated if the log
 * hasn't been disabled by a component's maximum log level setting or by
 * unsetting the global structured log handler.
 *
 * Each log 'item' may represent a deferred operation that has minimal cost
 * unless mongoc_structured_log_entry_message_as_bson is actually invoked.
 *
 * Calls implementation functions _mongoc_structured_log_should_log() before
 * building the table of item information and _mongoc_structured_log_with_entry()
 * once the table is built.
 */
#define mongoc_structured_log(_level, _component, ...) \
   _bsonDSL_eval (_mongoc_structured_log_with_end_of_list (_level, _component, __VA_ARGS__, end_of_list ()))

#define _mongoc_structured_log_with_end_of_list(_level, _component, _message, ...)                        \
   do {                                                                                                   \
      mongoc_structured_log_entry_t _entry = {                                                            \
         .envelope.level = (_level), .envelope.component = (_component), .envelope.message = (_message)}; \
      if (_mongoc_structured_log_should_log (&_entry.envelope)) {                                         \
         const mongoc_structured_log_builder_stage_t _builder[] = {                                       \
            _mongoc_structured_log_items_to_stages (__VA_ARGS__)};                                        \
         _entry.builder = _builder;                                                                       \
         _mongoc_structured_log_with_entry (&_entry);                                                     \
      }                                                                                                   \
   } while (0)

#define _mongoc_structured_log_items_to_stages(...) \
   _bsonDSL_mapMacro (_mongoc_structured_log_item_to_stages, ~, __VA_ARGS__)

#define _mongoc_structured_log_flag_expr(_action, _constant, _counter) | (_constant##_##_action)

#define _mongoc_structured_log_item_to_stages(_action, _constant, _counter) _mongoc_structured_log_item_##_action

#define _mongoc_structured_log_item_end_of_list() {.func = NULL},

/**
 * @def utf8(key, value)
 * @brief Structured log item, referencing a NUL-terminated utf8 string value.
 *
 * @param key Key as a const char * expression, or NULL to skip this item.
 * @param value UTF8 value as a const char * expression, or NULL for a null value.
 */
#define _mongoc_structured_log_item_utf8(_key_or_null, _value_utf8) \
   {.func = _mongoc_structured_log_append_utf8, .arg1.utf8 = (_key_or_null), .arg2.utf8 = (_value_utf8)},

/**
 * @def utf8_n(key, value, value_len)
 * @brief Structured log item, referencing a utf8 string value with explicit length.
 *
 * @param key Document key as a NULL-terminated const char * expression, or NULL to skip this item.
 * @param value UTF8 value as a const char * expression, or NULL for a null value. May have embedded NUL bytes.
 * @param value_len UTF8 value length in bytes, as an int32_t expression.
 */
#define _mongoc_structured_log_item_utf8_n(_key_literal, _value_utf8, _value_len) \
   _mongoc_structured_log_item_utf8_nn (_key_literal, strlen (_key_literal), _value_utf8, _value_len)

/**
 * @def utf8_nn(key, key_len, value, value_len)
 * @brief Structured log item, referencing a utf8 string with explicit key and value lengths.
 *
 * @param key Key as a NULL-terminated const char * expression, or NULL to skip this item.
 * @param value UTF8 value as a const char * expression, or NULL for a null value. May have embedded NUL bytes.
 * @param value_len UTF8 value length in bytes, as an int32_t expression.
 */
#define _mongoc_structured_log_item_utf8_nn(_key_or_null, _key_len, _value_utf8, _value_len)                     \
   {.func = _mongoc_structured_log_append_utf8_n_stage0, .arg1.utf8 = (_key_or_null), .arg2.int32 = (_key_len)}, \
      {.func = _mongoc_structured_log_append_utf8_n_stage1, .arg1.utf8 = (_value_utf8), .arg2.int32 = (_value_len)},

/**
 * @def int32(key, value)
 * @brief Structured log item, 32-bit integer
 *
 * @param key Key as a NULL-terminated const char * expression, or NULL to skip this item.
 * @param value Value as an int32_t expression.
 */
#define _mongoc_structured_log_item_int32(_key_or_null, _value_int32) \
   {.func = _mongoc_structured_log_append_int32, .arg1.utf8 = (_key_or_null), .arg2.int32 = (_value_int32)},

/**
 * @def int64(key, value)
 * @brief Structured log item, 64-bit integer
 *
 * @param key Key as a NULL-terminated const char * expression, or NULL to skip this item.
 * @param value Value as an int64_t expression.
 */
#define _mongoc_structured_log_item_int64(_key_or_null, _value_int64) \
   {.func = _mongoc_structured_log_append_int64, .arg1.utf8 = (_key_or_null), .arg2.int64 = (_value_int64)},

/**
 * @def boolean(key, value)
 * @brief Structured log item, boolean
 *
 * @param key Key as a NULL-terminated const char * expression, or NULL to skip this item.
 * @param value Value as a bool expression.
 */
#define _mongoc_structured_log_item_boolean(_key_or_null, _value_boolean) \
   {.func = _mongoc_structured_log_append_boolean, .arg1.utf8 = (_key_or_null), .arg2.boolean = (_value_boolean)},

/**
 * @def oid_as_hex(key, value)
 * @brief Structured log item, bson_oid_t converted to a hex string
 *
 * @param key Key as a NULL-terminated const char * expression, or NULL to skip this item.
 * @param value OID to convert as a const bson_oid_t * expression, or NULL for a null value.
 */
#define _mongoc_structured_log_item_oid_as_hex(_key_or_null, _value_oid) \
   {.func = _mongoc_structured_log_append_oid_as_hex, .arg1.utf8 = (_key_or_null), .arg2.oid = (_value_oid)},

/**
 * @def bson_as_json(key, value)
 * @brief Structured log item, bson_t converted to a JSON string
 *
 * Always uses relaxed extended JSON format, and the current applicable
 * maximum document length for structured logging.
 *
 * @param key Key as a NULL-terminated const char * expression, or NULL to skip this item.
 * @param value BSON to convert as a const bson_t * expression, or NULL for a null value.
 */
#define _mongoc_structured_log_item_bson_as_json(_key_or_null, _value_bson) \
   {.func = _mongoc_structured_log_append_bson_as_json, .arg1.utf8 = (_key_or_null), .arg2.bson = (_value_bson)},

/**
 * @def cmd(cmd, ...)
 * @brief Structured log item, mongoc_cmd_t fields with automatic redaction
 *
 * @param cmd Borrowed command reference, as a const mongo_cmd_t * expression. Required.
 * @param ... Fields to include. Order is not significant. Any of: COMMAND, DATABASE_NAME, COMMAND_NAME, OPERATION_ID.
 * */
#define _mongoc_structured_log_item_cmd(_cmd, ...) \
   {.func = _mongoc_structured_log_append_cmd,     \
    .arg1.cmd = (_cmd),                            \
    .arg2.cmd_flags =                              \
       (0 _bsonDSL_mapMacro (_mongoc_structured_log_flag_expr, MONGOC_STRUCTURED_LOG_CMD, __VA_ARGS__))},

/**
 * @def cmd_reply(cmd, reply)
 * @brief Structured log item, command reply for mongoc_cmd_t with automatic redaction
 *
 * @param cmd Borrowed command reference, as a const mongo_cmd_t * expression. Required.
 * @param reply Borrowed reference to reply document, as a const bson_t * expression. Required.
 */
#define _mongoc_structured_log_item_cmd_reply(_cmd, _reply_bson) \
   {.func = _mongoc_structured_log_append_cmd_reply, .arg1.cmd = (_cmd), .arg2.bson = (_reply_bson)},

/**
 * @def cmd_name_reply(cmd_name, reply)
 * @brief Structured log item, reply for named command with automatic redaction
 *
 * For cases where a mongo_cmd_t is not available; makes redaction decisions based
 * on command name but not body, so it's unsuitable for the "hello" reply.
 *
 * @param cmd Command name as a const char * expression. Required.
 * @param reply Borrowed reference to reply document, as a const bson_t * expression. Required.
 */
#define _mongoc_structured_log_item_cmd_name_reply(_cmd_name, _reply_bson) \
   {.func = _mongoc_structured_log_append_cmd_name_reply, .arg1.utf8 = (_cmd_name), .arg2.bson = (_reply_bson)},

/**
 * @def cmd_failure(cmd, reply, error)
 * @brief Structured log item, failure for mongoc_cmd_t with automatic redaction
 *
 * The 'error' is examined to determine whether this is a client-side or server-side failure.
 * The command's name and body may influence the reply's redaction.
 *
 * @param cmd Borrowed command reference, as a const mongo_cmd_t * expression. Required.
 * @param reply Borrowed reference to reply document, as a const bson_t * expression. Required.
 * @param error Borrowed reference to a libmongoc error, as a const bson_error_t * expression. Required.
 */
#define _mongoc_structured_log_item_cmd_failure(_cmd, _reply_bson, _error)                                     \
   {.func = _mongoc_structured_log_append_cmd_failure_stage0, .arg1.cmd = (_cmd), .arg2.bson = (_reply_bson)}, \
      {.func = _mongoc_structured_log_append_cmd_failure_stage1, .arg1.error = (_error)},

/**
 * @def cmd_name_failure(cmd_name, reply, error)
 * @brief Structured log item, failure for named command with automatic redaction
 *
 * For cases where a mongo_cmd_t is not available; makes redaction decisions based
 * on command name but not body, so it's unsuitable for "hello" errors.
 *
 * The 'error' is examined to determine whether this is a client-side or server-side failure.
 * The command's name and body may influence the reply's redaction.
 *
 * @param cmd Command name as a const char * expression. Required.
 * @param reply Borrowed reference to reply document, as a const bson_t * expression. Required.
 * @param error Borrowed reference to a libmongoc error, as a const bson_error_t * expression. Required.
 */
#define _mongoc_structured_log_item_cmd_name_failure(_cmd_name, _reply_bson, _error) \
   {.func = _mongoc_structured_log_append_cmd_name_failure_stage0,                   \
    .arg1.utf8 = (_cmd_name),                                                        \
    .arg2.bson = (_reply_bson)},                                                     \
      {.func = _mongoc_structured_log_append_cmd_name_failure_stage1, .arg1.error = (_error)},

/**
 * @def server_description(sd, ...)
 * @brief Structured log item, mongoc_server_description_t fields
 *
 * @param cmd Borrowed server description reference, as a const mongoc_server_description_t * expression. Required.
 * @param ... Fields to include. Order is not significant. Any of: SERVER_HOST, SERVER_PORT, SERVER_CONNECTION_ID,
 * SERVICE_ID.
 * */
#define _mongoc_structured_log_item_server_description(_server_description, ...) \
   {.func = _mongoc_structured_log_append_server_description,                    \
    .arg1.server_description = (_server_description),                            \
    .arg2.server_description_flags = (0 _bsonDSL_mapMacro (                      \
       _mongoc_structured_log_flag_expr, MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION, __VA_ARGS__))},

/**
 * @def monotonic_time_duration(duration)
 * @brief Structured log item, standard format for a duration in monotonic time.
 * @param duration Duration in microseconds, as an int64_t expression.
 *
 * @todo Is the (CLAM) spec asking for only the highest resolution available, or that plus milliseconds?
 * */
#define _mongoc_structured_log_item_monotonic_time_duration(_duration)              \
   _mongoc_structured_log_item_int32 ("durationMS", (int32_t) ((_duration) / 1000)) \
      _mongoc_structured_log_item_int64 ("durationMicros", (_duration))

typedef struct mongoc_structured_log_builder_stage_t mongoc_structured_log_builder_stage_t;

typedef const mongoc_structured_log_builder_stage_t *(*mongoc_structured_log_builder_func_t) (
   bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

typedef enum {
   MONGOC_STRUCTURED_LOG_CMD_COMMAND = (1 << 0),
   MONGOC_STRUCTURED_LOG_CMD_DATABASE_NAME = (1 << 1),
   MONGOC_STRUCTURED_LOG_CMD_COMMAND_NAME = (1 << 2),
   MONGOC_STRUCTURED_LOG_CMD_OPERATION_ID = (1 << 3),
} mongoc_structured_log_cmd_flags_t;

typedef enum {
   MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_HOST = (1 << 0),
   MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_PORT = (1 << 1),
   MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_CONNECTION_ID = (1 << 2),
   MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVICE_ID = (1 << 3),
} mongoc_structured_log_server_description_flags_t;

struct mongoc_structured_log_builder_stage_t {
   // Why "stages" instead of a variable size argument list per item?
   // This approach keeps function pointers and other types of data
   // separated, reducing opportunities for malicious control flow.
   // Most items are one stage. Items that need more arguments can use
   // multiple consecutive stages, leaving the extra stages' function
   // pointers unused and set to placeholder values which can be checked.
   mongoc_structured_log_builder_func_t func; // NULL sentinel here
   union {
      const bson_error_t *error;
      const mongoc_cmd_t *cmd;
      const mongoc_server_description_t *server_description;
      const void *utf8;
   } arg1;
   union {
      bool boolean;
      bson_oid_t *oid;
      const bson_t *bson;
      const void *utf8;
      int32_t int32;
      int64_t int64;
      mongoc_structured_log_cmd_flags_t cmd_flags;
      mongoc_structured_log_server_description_flags_t server_description_flags;
   } arg2;
   // Avoid adding an arg3, prefer to use additional stages
};

typedef struct mongoc_structured_log_envelope_t {
   mongoc_structured_log_level_t level;
   mongoc_structured_log_component_t component;
   const char *message;
} mongoc_structured_log_envelope_t;

struct mongoc_structured_log_entry_t {
   mongoc_structured_log_envelope_t envelope;
   const mongoc_structured_log_builder_stage_t *builder; // Required
};

void
_mongoc_structured_log_init (void);

char *
mongoc_structured_log_document_to_json (const bson_t *document, size_t *length);

void
mongoc_structured_log_get_handler (mongoc_structured_log_func_t *log_func, void **user_data);

bool
_mongoc_structured_log_should_log (const mongoc_structured_log_envelope_t *envelope);

void
_mongoc_structured_log_with_entry (const mongoc_structured_log_entry_t *entry);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_utf8 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_utf8_n_stage0 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_utf8_n_stage1 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_int32 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_int64 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_boolean (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_oid_as_hex (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_bson_as_json (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_reply (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_name_reply (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_failure_stage0 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_failure_stage1 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_name_failure_stage0 (bson_t *bson,
                                                       const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_name_failure_stage1 (bson_t *bson,
                                                       const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_server_description (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

BSON_END_DECLS

#endif /* MONGOC_STRUCTURED_LOG_PRIVATE_H */
