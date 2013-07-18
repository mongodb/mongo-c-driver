/*
 * Copyright 2013 10gen Inc.
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


#ifndef MONGOC_COLLECTION_H
#define MONGOC_COLLECTION_H


#include <bson.h>

#include "mongoc-flags.h"
#include "mongoc-cursor.h"
#include "mongoc-read-prefs.h"
#include "mongoc-write-concern.h"


BSON_BEGIN_DECLS


typedef struct _mongoc_collection_t mongoc_collection_t;


void                          mongoc_collection_destroy           (mongoc_collection_t    *collection);
mongoc_cursor_t              *mongoc_collection_command           (mongoc_collection_t    *collection,
                                                                   mongoc_query_flags_t    flags,
                                                                   bson_uint32_t           skip,
                                                                   bson_uint32_t           n_return,
                                                                   const bson_t           *query,
                                                                   const bson_t           *fields,
                                                                   mongoc_read_prefs_t    *read_prefs);
bson_bool_t                   mongoc_collection_command_simple    (mongoc_collection_t    *collection,
                                                                   const bson_t           *command,
                                                                   mongoc_read_prefs_t    *read_prefs,
                                                                   bson_t                 *reply,
                                                                   bson_error_t           *error);
bson_int64_t                  mongoc_collection_count             (mongoc_collection_t    *collection,
                                                                   mongoc_query_flags_t    flags,
                                                                   const bson_t           *query,
                                                                   bson_int64_t            skip,
                                                                   bson_int64_t            limit,
                                                                   mongoc_read_prefs_t    *read_prefs,
                                                                   bson_error_t           *error);
bson_bool_t                   mongoc_collection_drop              (mongoc_collection_t    *collection,
                                                                   bson_error_t           *error);
bson_bool_t                   mongoc_collection_drop_index        (mongoc_collection_t    *collection,
                                                                   const char             *index_name,
                                                                   bson_error_t           *error);
mongoc_cursor_t              *mongoc_collection_find              (mongoc_collection_t    *collection,
                                                                   mongoc_query_flags_t    flags,
                                                                   bson_uint32_t           skip,
                                                                   bson_uint32_t           n_return,
                                                                   const bson_t           *query,
                                                                   const bson_t           *fields,
                                                                   mongoc_read_prefs_t    *read_prefs);
bson_bool_t                   mongoc_collection_insert            (mongoc_collection_t    *collection,
                                                                   mongoc_insert_flags_t   flags,
                                                                   const bson_t           *document,
                                                                   mongoc_write_concern_t *write_concern,
                                                                   bson_t                 *result,
                                                                   bson_error_t           *error);
bson_bool_t                   mongoc_collection_update            (mongoc_collection_t    *collection,
                                                                   mongoc_update_flags_t   flags,
                                                                   const bson_t           *selector,
                                                                   const bson_t           *update,
                                                                   mongoc_write_concern_t *write_concern,
                                                                   bson_error_t           *error);
bson_bool_t                   mongoc_collection_delete            (mongoc_collection_t    *collection,
                                                                   mongoc_delete_flags_t   flags,
                                                                   const bson_t           *selector,
                                                                   mongoc_write_concern_t *write_concern,
                                                                   bson_error_t           *error);
const mongoc_read_prefs_t    *mongoc_collection_get_read_prefs    (const mongoc_collection_t    *collection);
void                          mongoc_collection_set_read_prefs    (mongoc_collection_t          *collection,
                                                                   const mongoc_read_prefs_t    *read_prefs);
const mongoc_write_concern_t *mongoc_collection_get_write_concern (const mongoc_collection_t    *collection);
void                          mongoc_collection_set_write_concern (mongoc_collection_t          *collection,
                                                                   const mongoc_write_concern_t *write_concern);


BSON_END_DECLS


#endif /* MONGOC_COLLECTION_H */
