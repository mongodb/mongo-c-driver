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
   MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND = 0,
   MONGOC_STRUCTURED_LOG_COMPONENT_TOPOLOGY = 1,
   MONGOC_STRUCTURED_LOG_COMPONENT_SERVER_SELECTION = 2,
   MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION = 3,
} mongoc_structured_log_component_t;

typedef struct mongoc_structured_log_entry_t mongoc_structured_log_entry_t;

typedef void (*mongoc_structured_log_func_t) (const mongoc_structured_log_entry_t *entry, void *user_data);

/**
 * mongoc_structured_log_set_handler:
 * @log_func: A function to handle structured messages.
 * @user_data: User data for @log_func.
 *
 * Sets the function to be called to handle structured log messages.
 *
 * The callback is given a mongoc_structured_log_entry_t* as a handle for
 * obtaining additional information about the log message.
 *
 * The entry pointer is only valid during a callback, because it's a low
 * cost reference to temporary data. When the log message is extracted
 * as a bson_t a new owned copy of this data must be made, and deferred
 * transformations like bson_as_json take place.
 *
 * Callbacks may use mongoc_structured_log_entry_get_level and
 * mongoc_structured_log_entry_get_component to filter log messages, and
 * selectively call mongoc_structured_log_entry_message_as_bson to create
 * a bson_t for log messages only when needed.
 */
MONGOC_EXPORT (void)
mongoc_structured_log_set_handler (mongoc_structured_log_func_t log_func, void *user_data);

/**
 * mongoc_structured_log_set_max_level_for_component:
 * @level: Maximum log level for this component.
 * @component: A logging component to set the level limit for.
 *
 * Sets the maximum log level per-component. Only log messages at or below
 * this severity level will be passed to mongoc_structured_log_func_t.
 *
 * By default, each component's log level comes from the environment
 * variables `MONGOC_LOG_<component>` captured during mongoc_init().
 */
MONGOC_EXPORT (void)
mongoc_structured_log_set_max_level_for_component (mongoc_structured_log_component_t component,
                                                   mongoc_structured_log_level_t level);

/**
 * mongoc_structured_log_set_max_level_for_all_components:
 * @level: Maximum log level for all components.
 *
 * Sets all per-component maximum log levels to the same value.
 * Only log messages at or below this severity level will be passed to
 * mongoc_structured_log_func_t. Effective even for logging components not
 * known at compile-time.
 */
MONGOC_EXPORT (void)
mongoc_structured_log_set_max_level_for_all_components (mongoc_structured_log_level_t level);

/**
 * mongoc_structured_log_get_max_level_for_component:
 * @component: A logging component to check the log level limit for.
 *
 * Returns the current maximum log level for one component.
 */
MONGOC_EXPORT (mongoc_structured_log_level_t)
mongoc_structured_log_get_max_level_for_component (mongoc_structured_log_component_t component);

/**
 * mongoc_structured_log_entry_message_as_bson:
 * @entry: A log entry to extract the message from.
 *
 * Returns the structured message as a new allocated bson_t
 * that must be freed by bson_destroy().
 */
MONGOC_EXPORT (bson_t *)
mongoc_structured_log_entry_message_as_bson (const mongoc_structured_log_entry_t *entry);

/**
 * mongoc_structured_log_entry_get_level:
 * @entry: A log entry to read the level from.
 *
 * Returns the severity level of the structured log entry.
 */
MONGOC_EXPORT (mongoc_structured_log_level_t)
mongoc_structured_log_entry_get_level (const mongoc_structured_log_entry_t *entry);

/**
 * mongoc_structured_log_entry_get_component:
 * @entry: A log entry to read the component from.
 *
 * Returns the component of the structured log entry.
 */
MONGOC_EXPORT (mongoc_structured_log_component_t)
mongoc_structured_log_entry_get_component (const mongoc_structured_log_entry_t *entry);

/**
 * mongoc_structured_log_get_level_name:
 * @level: A log level code.
 *
 * Returns the canonical name for a log level as a constant string that does
 * not need to be deallocated, or NULL if the level has no known name.
 */
MONGOC_EXPORT (const char *)
mongoc_structured_log_get_level_name (mongoc_structured_log_level_t level);

/**
 * mongoc_structured_log_get_component_name:
 * @level: A log level code.
 *
 * Returns the canonical name for a component as a constant string that does
 * not need to be deallocated, or NULL if the level has no known name.
 */
MONGOC_EXPORT (const char *)
mongoc_structured_log_get_component_name (mongoc_structured_log_component_t component);

BSON_END_DECLS

#endif /* MONGOC_STRUCTURED_LOG_H */
