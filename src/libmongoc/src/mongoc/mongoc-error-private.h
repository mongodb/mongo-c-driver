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

#include <bson/bson.h>
#include <stddef.h>

BSON_BEGIN_DECLS

typedef enum {
   MONGOC_READ_ERR_NONE,
   MONGOC_READ_ERR_OTHER,
   MONGOC_READ_ERR_RETRY,
} mongoc_read_err_type_t;

mongoc_read_err_type_t
_mongoc_read_error_get_type (bool cmd_ret,
                             const bson_error_t *cmd_err,
                             const bson_t *reply);

void
_mongoc_error_copy_labels_and_upsert (const bson_t *src,
                                      bson_t *dst,
                                      char *label);

void
_mongoc_write_error_handle_labels (bool cmd_ret,
                                   const bson_error_t *cmd_err,
                                   bson_t *reply,
                                   int32_t server_max_wire_version);

bool
_mongoc_error_is_shutdown (bson_error_t *error);

bool
_mongoc_error_is_not_master (bson_error_t *error);

bool
_mongoc_error_is_state_change (bson_error_t *error);

bool
_mongoc_error_is_network (const bson_error_t *error);

BSON_END_DECLS
