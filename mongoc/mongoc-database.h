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


BSON_BEGIN_DECLS


typedef struct _mongoc_database_t mongoc_database_t;


void
mongoc_database_destroy (mongoc_database_t *database);


mongoc_cursor_t *
mongoc_database_command (mongoc_database_t    *database,
                         mongoc_query_flags_t  flags,
                         bson_uint32_t         skip,
                         bson_uint32_t         n_return,
                         const bson_t         *command,
                         const bson_t         *fields,
                         const bson_t         *options);


BSON_END_DECLS


#endif /* MONGOC_DATABASE_H */
