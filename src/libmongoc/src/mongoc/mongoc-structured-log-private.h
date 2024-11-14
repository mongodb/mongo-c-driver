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

#define mongoc_structured_log(_level, _component, ...) \
   _mongoc_structured_log_with_end_of_list (_level, _component, __VA_ARGS__, end_of_list ())

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
   _bsonDSL_eval (_bsonDSL_mapMacro (_mongoc_structured_log_item_to_stages, ~, __VA_ARGS__))

#define _mongoc_structured_log_flag_expr(_action, _constant, _counter) | (_constant##_##_action)

#define _mongoc_structured_log_item_to_stages(_action, _constant, _counter) _mongoc_structured_log_item_##_action

#define _mongoc_structured_log_item_end_of_list() {.func = NULL},

#define _mongoc_structured_log_item_utf8(_key_or_null, _value_utf8) \
   {.func = _mongoc_structured_log_append_utf8, .arg1.utf8 = (_key_or_null), .arg2.utf8 = (_value_utf8)},

#define _mongoc_structured_log_item_utf8_n(_key_literal, _value_utf8, _value_len) \
   _mongoc_structured_log_item_utf8_nn (_key_literal, strlen (_key_literal), _value_utf8, _value_len)

#define _mongoc_structured_log_item_utf8_nn(_key_or_null, _key_len, _value_utf8, _value_len)                     \
   {.func = _mongoc_structured_log_append_utf8_n_stage0, .arg1.utf8 = (_key_or_null), .arg2.int32 = (_key_len)}, \
      {.func = _mongoc_structured_log_append_utf8_n_stage1, .arg1.utf8 = (_value_utf8), .arg2.int32 = (_value_len)},

#define _mongoc_structured_log_item_int32(_key_or_null, _value_int32) \
   {.func = _mongoc_structured_log_append_int32, .arg1.utf8 = (_key_or_null), .arg2.int32 = (_value_int32)},

#define _mongoc_structured_log_item_int64(_key_or_null, _value_int64) \
   {.func = _mongoc_structured_log_append_int64, .arg1.utf8 = (_key_or_null), .arg2.int64 = (_value_int64)},

#define _mongoc_structured_log_item_boolean(_key_or_null, _value_boolean) \
   {.func = _mongoc_structured_log_append_boolean, .arg1.utf8 = (_key_or_null), .arg2.boolean = (_value_boolean)},

#define _mongoc_structured_log_item_oid_as_hex(_key_or_null, _value_oid) \
   {.func = _mongoc_structured_log_append_oid_as_hex, .arg1.utf8 = (_key_or_null), .arg2.oid = (_value_oid)},

#define _mongoc_structured_log_item_bson_as_json(_key_or_null, _value_bson) \
   {.func = _mongoc_structured_log_append_bson_as_json, .arg1.utf8 = (_key_or_null), .arg2.bson = (_value_bson)},

#define _mongoc_structured_log_item_cmd(_cmd, ...) \
   {.func = _mongoc_structured_log_append_cmd,     \
    .arg1.cmd = (_cmd),                            \
    .arg2.cmd_flags =                              \
       (0 _bsonDSL_mapMacro (_mongoc_structured_log_flag_expr, MONGOC_STRUCTURED_LOG_CMD, __VA_ARGS__))},

#define _mongoc_structured_log_item_cmd_reply(_cmd_name, _reply_bson) \
   {.func = _mongoc_structured_log_append_cmd_reply, .arg1.utf8 = (_cmd_name), .arg2.bson = (_reply_bson)},

#define _mongoc_structured_log_item_cmd_failure(_cmd_name, _reply_bson, _error)                                      \
   {.func = _mongoc_structured_log_append_cmd_failure_stage0, .arg1.utf8 = (_cmd_name), .arg2.bson = (_reply_bson)}, \
      {.func = _mongoc_structured_log_append_cmd_failure_stage1, .arg1.error = (_error)},

#define _mongoc_structured_log_item_server_description(_server_description, ...) \
   {.func = _mongoc_structured_log_append_server_description,                    \
    .arg1.server_description = (_server_description),                            \
    .arg2.server_description_flags = (0 _bsonDSL_mapMacro (                      \
       _mongoc_structured_log_flag_expr, MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION, __VA_ARGS__))},

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
_mongoc_structured_log_append_cmd_failure_stage0 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_cmd_failure_stage1 (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

const mongoc_structured_log_builder_stage_t *
_mongoc_structured_log_append_server_description (bson_t *bson, const mongoc_structured_log_builder_stage_t *stage);

BSON_END_DECLS

#endif /* MONGOC_STRUCTURED_LOG_PRIVATE_H */
