/*
 * Copyright 2015 MongoDB, Inc.
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

#ifndef MONGOC_STRUCTRURED_LOG_PRIVATE_H
#define MONGOC_STRUCTRURED_LOG_PRIVATE_H

typedef void (*mongoc_structured_log_build_context_t) (bson_t *context, va_list *context_data);

struct _mongoc_structured_log_entry_t {
   mongoc_structured_log_level_t level;
   mongoc_structured_log_component_t component;
   const char* message;
   mongoc_structured_log_build_context_t build_context;
   va_list *context_data;
   bson_t *context;
   bool context_built;
};

void
mongoc_structured_log (mongoc_structured_log_level_t level,
                       mongoc_structured_log_component_t component,
                       const char *message,
                       mongoc_structured_log_build_context_t build_context,
                       ...);

void
mongoc_structured_log_command_started (mongoc_cmd_t *cmd,
                                       uint32_t request_id,
                                       uint32_t driver_connection_id,
                                       uint32_t server_connection_id,
                                       bool explicit_session);

#define MONGOC_STRUCTURED_LOG_COMMAND_STARTED()

#endif /* MONGOC_STRUCTURED_LOG_PRIVATE_H */
