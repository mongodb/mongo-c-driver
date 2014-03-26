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


#ifndef MONGOC_WRITE_COMMAND_PRIVATE_H
#define MONGOC_WRITE_COMMAND_PRIVATE_H


#include <bson.h>

#include "mongoc-client.h"
#include "mongoc-write-concern.h"


BSON_BEGIN_DECLS


#define MONGOC_WRITE_COMMAND_DELETE 0
#define MONGOC_WRITE_COMMAND_INSERT 1
#define MONGOC_WRITE_COMMAND_UPDATE 2


typedef struct
{
   int type;
   union {
      struct {
         uint8_t   ordered : 1;
         uint8_t   multi : 1;
         bson_t   *selector;
      } delete;
      struct {
         uint8_t   ordered : 1;
         bson_t   *documents;
         uint32_t  n_documents;
      } insert;
      struct {
         uint8_t   ordered : 1;
         uint8_t   upsert : 1;
         uint8_t   multi  : 1;
         bson_t   *selector;
         bson_t   *update;
      } update;
   } u;
} mongoc_write_command_t;


void _mongoc_write_command_destroy     (mongoc_write_command_t       *command);
void _mongoc_write_command_init_insert (mongoc_write_command_t       *command,
                                        const bson_t * const         *documents,
                                        uint32_t                      n_documents,
                                        bool                          ordered);
void _mongoc_write_command_init_delete (mongoc_write_command_t       *command,
                                        const bson_t                 *selector,
                                        bool                          multi,
                                        bool                          ordered);
void _mongoc_write_command_init_update (mongoc_write_command_t       *command,
                                        const bson_t                 *selector,
                                        const bson_t                 *update,
                                        bool                          upsert,
                                        bool                          multi,
                                        bool                          ordered);
bool _mongoc_write_command_execute     (mongoc_write_command_t       *command,
                                        mongoc_client_t              *client,
                                        uint32_t                      hint,
                                        const char                   *database,
                                        const char                   *collection,
                                        const mongoc_write_concern_t *write_concern,
                                        bson_t                       *reply,
                                        bson_error_t                 *error);


BSON_END_DECLS


#endif /* MONGOC_WRITE_COMMAND_PRIVATE_H */
