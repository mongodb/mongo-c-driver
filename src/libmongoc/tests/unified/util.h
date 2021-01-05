/*
 * Copyright 2020-present MongoDB, Inc.
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

#ifndef UNIFIED_UTIL_H
#define UNIFIED_UTIL_H

#include "mongoc/mongoc.h"

mongoc_write_concern_t * bson_to_write_concern (bson_t *bson, bson_error_t* error);
mongoc_read_concern_t * bson_to_read_concern (bson_t *bson, bson_error_t* error);
mongoc_read_prefs_t * bson_to_read_prefs (bson_t *bson, bson_error_t* error);

#endif /* UNIFIED_UTIL_H */
