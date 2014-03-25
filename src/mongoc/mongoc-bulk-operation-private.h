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


BSON_BEGIN_DECLS


struct _mongoc_bulk_operation_t
{
   char            *collection;
   mongoc_client_t *client;
   bool             ordered;
   uint32_t         hint;
   mongoc_array_t   commands;
};


typedef enum
{
   MONGOC_BULK_COMMAND_INSERT = 1,
   MONGOC_BULK_COMMAND_UPDATE,
   MONGOC_BULK_COMMAND_DELETE,
   MONGOC_BULK_COMMAND_REPLACE,
} mongoc_bulk_command_type_t;


typedef struct
{
   int type;
   union {
      struct {
         bson_t *document;
      } insert;
      struct {
         uint8_t   upsert : 1;
         uint8_t   multi  : 1;
         bson_t   *selector;
         bson_t   *document;
      } update;
      struct {
         uint8_t  multi : 1;
         bson_t  *selector;
      } delete;
      struct {
         uint8_t  upsert : 1;
         bson_t *selector;
         bson_t *document;
      } replace;
   } u;
} mongoc_bulk_command_t;


mongoc_bulk_operation_t *_mongoc_bulk_operation_new (mongoc_client_t              *client,
                                                     const char                   *collection,
                                                     uint32_t                      hint,
                                                     bool                          ordered,
                                                     const mongoc_write_concern_t *write_concern);


BSON_END_DECLS


#endif /* MONGOC_BULK_OPERATION_PRIVATE_H */
