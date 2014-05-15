/*
 * Copyright 2014 MongoDB, Inc.
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


#ifndef MONGOC_BULK_OPERATION_H
#define MONGOC_BULK_OPERATION_H


#include <bson.h>

#include "mongoc-write-concern.h"


BSON_BEGIN_DECLS


typedef struct _mongoc_bulk_operation_t mongoc_bulk_operation_t;


void mongoc_bulk_operation_destroy     (mongoc_bulk_operation_t       *bulk);
bool mongoc_bulk_operation_execute     (mongoc_bulk_operation_t       *bulk,
                                        bson_t                        *reply,
                                        bson_error_t                  *error);
void mongoc_bulk_operation_delete      (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector)
   BSON_GNUC_DEPRECATED_FOR (mongoc_bulk_operation_remove);
void mongoc_bulk_operation_delete_one  (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector)
   BSON_GNUC_DEPRECATED_FOR (mongoc_bulk_operation_remove_one);
void mongoc_bulk_operation_insert      (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *document);
void mongoc_bulk_operation_remove      (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector);
void mongoc_bulk_operation_remove_one  (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector);
void mongoc_bulk_operation_replace_one (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector,
                                        const bson_t                  *document,
                                        bool                           upsert);
void mongoc_bulk_operation_update      (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector,
                                        const bson_t                  *document,
                                        bool                           upsert);
void mongoc_bulk_operation_update_one  (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector,
                                        const bson_t                  *document,
                                        bool                           upsert);


BSON_END_DECLS


#endif /* MONGOC_BULK_OPERATION_H */
