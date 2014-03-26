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


#ifndef MONGOC_BULK_OPERATION_PRIVATE_H
#define MONGOC_BULK_OPERATION_PRIVATE_H


#include "mongoc-array-private.h"
#include "mongoc-client.h"
#include "mongoc-write-command-private.h"


BSON_BEGIN_DECLS


struct _mongoc_bulk_operation_t
{
   char                   *database;
   char                   *collection;
   mongoc_client_t        *client;
   mongoc_write_concern_t *write_concern;
   bool                    ordered;
   bool                    omit_n_modified;
   mongoc_array_t          commands;
   uint32_t                hint;
   uint32_t                n_modified;
   uint32_t                n_upserted;
   uint32_t                n_matched;
   uint32_t                n_removed;
   uint32_t                n_inserted;
   uint32_t                offset;
   bson_t                 *upserted;
   bson_t                 *write_errors;
   bson_t                 *write_concern_errors;
};


mongoc_bulk_operation_t *_mongoc_bulk_operation_new (mongoc_client_t              *client,
                                                     const char                   *database,
                                                     const char                   *collection,
                                                     uint32_t                      hint,
                                                     bool                          ordered,
                                                     const mongoc_write_concern_t *write_concern);


BSON_END_DECLS


#endif /* MONGOC_BULK_OPERATION_PRIVATE_H */
