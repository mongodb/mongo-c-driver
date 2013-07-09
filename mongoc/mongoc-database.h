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


#ifndef MONGOC_DATABASE_H
#define MONGOC_DATABASE_H


#include <bson.h>

#include "mongoc-cursor.h"
#include "mongoc-flags.h"
#include "mongoc-read-prefs.h"


BSON_BEGIN_DECLS


typedef struct _mongoc_database_t mongoc_database_t;


void                          mongoc_database_destroy           (mongoc_database_t            *database);
mongoc_cursor_t              *mongoc_database_command           (mongoc_database_t            *database,
                                                                 mongoc_query_flags_t          flags,
                                                                 bson_uint32_t                 skip,
                                                                 bson_uint32_t                 n_return,
                                                                 const bson_t                 *command,
                                                                 const bson_t                 *fields,
                                                                 mongoc_read_prefs_t          *read_prefs);
bson_bool_t                   mongoc_database_command_simple    (mongoc_database_t            *database,
                                                                 const bson_t                 *command,
                                                                 bson_error_t                 *error);
bson_bool_t                   mongoc_database_drop              (mongoc_database_t            *database,
                                                                 bson_error_t                 *error);
const mongoc_read_prefs_t    *mongoc_database_get_read_prefs    (const mongoc_database_t      *database);
void                          mongoc_database_set_read_prefs    (mongoc_database_t            *database,
                                                                 const mongoc_read_prefs_t    *read_prefs);
const mongoc_write_concern_t *mongoc_database_get_write_concern (const mongoc_database_t      *database);
void                          mongoc_database_set_write_concern (mongoc_database_t            *database,
                                                                 const mongoc_write_concern_t *write_concern);


BSON_END_DECLS


#endif /* MONGOC_DATABASE_H */
