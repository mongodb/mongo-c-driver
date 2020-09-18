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

#include "mongoc-prelude.h"

#ifndef MONGOC_STRUCTURED_LOG_H
#define MONGOC_STRUCTURED_LOG_H

#include <bson/bson.h>

#include "mongoc-macros.h"

BSON_BEGIN_DECLS

typedef enum {
   MONGOC_STRUCTURED_LOG_LEVEL_EMERGENCY = 0,
   MONGOC_STRUCTURED_LOG_LEVEL_ALERT = 1,
   MONGOC_STRUCTURED_LOG_LEVEL_CRITICAL = 2,
   MONGOC_STRUCTURED_LOG_LEVEL_ERROR = 3,
   MONGOC_STRUCTURED_LOG_LEVEL_WARNING = 4,
   MONGOC_STRUCTURED_LOG_LEVEL_NOTICE = 5,
   MONGOC_STRUCTURED_LOG_LEVEL_INFO = 6,
   MONGOC_STRUCTURED_LOG_LEVEL_DEBUG = 7,
   MONGOC_STRUCTURED_LOG_LEVEL_TRACE = 8,
} mongoc_structured_log_level_t;

typedef enum {
   MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
   MONGOC_STRUCTURED_LOG_COMPONENT_SDAM,
   MONGOC_STRUCTURED_LOG_COMPONENT_SERVER_SELECTION,
   MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION,
} mongoc_structured_log_component_t;

typedef struct _mongoc_structured_log_entry_t mongoc_structured_log_entry_t;

typedef void (*mongoc_structured_log_func_t) (mongoc_structured_log_entry_t *entry,
                                              void *user_data);

MONGOC_EXPORT (void)
mongoc_structured_log_set_handler (mongoc_structured_log_func_t log_func, void *user_data);

MONGOC_EXPORT (const bson_t*)
mongoc_structured_log_entry_get_message (mongoc_structured_log_entry_t *entry);

MONGOC_EXPORT (mongoc_structured_log_level_t)
mongoc_structured_log_entry_get_level (const mongoc_structured_log_entry_t *entry);

MONGOC_EXPORT (mongoc_structured_log_component_t)
mongoc_structured_log_entry_get_component (const mongoc_structured_log_entry_t *entry);

BSON_END_DECLS

#endif /* MONGOC_STRUCTURED_LOG_H */
