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

#include <mongoc/mongoc-prelude.h>

#ifndef MONGOC_FIND_AND_MODIFY_PRIVATE_H
#define MONGOC_FIND_AND_MODIFY_PRIVATE_H

#include <mongoc/mongoc-write-command-private.h>

#include <bson/bson.h>

BSON_BEGIN_DECLS

struct _mongoc_find_and_modify_opts_t {
   bson_t *sort;
   bson_t *update;
   bson_t *fields;
   mongoc_find_and_modify_flags_t flags;
   bool bypass_document_validation;
   uint32_t max_time_ms;
   bson_t extra;
};


BSON_END_DECLS


#endif /* MONGOC_FIND_AND_MODIFY_PRIVATE_H */
