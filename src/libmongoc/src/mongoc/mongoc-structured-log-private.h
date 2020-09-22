/*
 * Copyright 2020 MongoDB, Inc.
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
#include "mongoc-structured-log.h"
#include "mongoc-cmd-private.h"
#include "mongoc-structured-log-command-private.h"

#ifndef MONGOC_STRUCTRURED_LOG_PRIVATE_H
#define MONGOC_STRUCTRURED_LOG_PRIVATE_H

#define MONGOC_STRUCTURED_LOG_DEFAULT_LEVEL MONGOC_STRUCTURED_LOG_LEVEL_WARNING;

typedef void (*mongoc_structured_log_build_message_t) (
   mongoc_structured_log_entry_t *entry);

struct _mongoc_structured_log_entry_t {
   mongoc_structured_log_level_t level;
   mongoc_structured_log_component_t component;
   const char *message;
   bson_t *structured_message;
   mongoc_structured_log_build_message_t build_message_func;
   union {
      _mongoc_structured_log_command_t *command;
   };
};

void
mongoc_structured_log (mongoc_structured_log_level_t level,
                       mongoc_structured_log_component_t component,
                       const char *message,
                       mongoc_structured_log_build_message_t build_message_func,
                       void *structured_message_data);

void
_mongoc_structured_log_get_handler (mongoc_structured_log_func_t *log_func,
                                    void **user_data);

#endif /* MONGOC_STRUCTURED_LOG_PRIVATE_H */
