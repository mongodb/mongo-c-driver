/*
 * Copyright 2013 MongoDB Inc.
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


#ifndef MONGOC_GRIDFS_H
#define MONGOC_GRIDFS_H

#include "mongoc-stream.h"
#include "mongoc-gridfs-file.h"
#include "mongoc-gridfs-file-list.h"

#include <bson.h>

BSON_BEGIN_DECLS

typedef struct _mongoc_gridfs mongoc_gridfs_t;

mongoc_gridfs_file_t *
mongoc_gridfs_create_file_from_stream (mongoc_gridfs_t          *gridfs,
                                       mongoc_stream_t          *stream,
                                       mongoc_gridfs_file_opt_t *opt);

mongoc_gridfs_file_t *
mongoc_gridfs_create_file (mongoc_gridfs_t          *gridfs,
                           mongoc_gridfs_file_opt_t *opt);


mongoc_gridfs_file_list_t *
mongoc_gridfs_find (mongoc_gridfs_t *gridfs,
                    const bson_t    *query);


mongoc_gridfs_file_t *
mongoc_gridfs_find_one (mongoc_gridfs_t *gridfs,
                        const bson_t    *query);


mongoc_gridfs_file_t *
mongoc_gridfs_find_one_by_filename (mongoc_gridfs_t *gridfs,
                                    const char      *filename);


bson_bool_t
mongoc_gridfs_drop (mongoc_gridfs_t *gridfs,
                    bson_error_t    *error);

void
mongoc_gridfs_destroy (mongoc_gridfs_t *gridfs);

BSON_END_DECLS


#endif /* MONGOC_GRIDFS_H */
