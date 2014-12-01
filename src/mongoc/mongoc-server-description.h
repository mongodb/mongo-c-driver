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

#ifndef MONGOC_SERVER_DESCRIPTION_H
#define MONGOC_SERVER_DESCRIPTION_H

#include <bson.h>

#include "mongoc-array-private.h"
#include "mongoc-host-list.h"

#define MONGOC_SERVER_DESCRIPTION_TYPES 8

typedef enum
   {
      MONGOC_SERVER_STANDALONE,
      MONGOC_SERVER_MONGOS,
      MONGOC_SERVER_POSSIBLE_PRIMARY,
      MONGOC_SERVER_RS_PRIMARY,
      MONGOC_SERVER_RS_SECONDARY,
      MONGOC_SERVER_RS_ARBITER,
      MONGOC_SERVER_RS_OTHER,
      MONGOC_SERVER_RS_GHOST,
      MONGOC_SERVER_UNKNOWN,
   } mongoc_server_description_type_t;

typedef struct _mongoc_server_description_t mongoc_server_description_t;

struct _mongoc_server_description_t
{
   mongoc_server_description_t     *next;
   int32_t                          id;
   char                            *set_name;
   char                            *connection_address;
   mongoc_host_list_t               host;
   char                            *error; // TODO: what type should this be?
   int32_t                          round_trip_time;
   mongoc_server_description_type_t type;
   int32_t                          min_wire_version;
   int32_t                          max_wire_version;

   char                           **rs_members;

   bson_t                           tags;
   char                            *current_primary;
   int32_t                          max_write_batch_size;
};

void _mongoc_server_description_init          (mongoc_server_description_t     *description,
                                               const char                      *address,
                                               int32_t                          id);
void _mongoc_server_description_destroy       (mongoc_server_description_t     *description);
bool _mongoc_server_description_has_rs_member (mongoc_server_description_t     *description,
                                               const char                      *address);
void _mongoc_server_description_set_state     (mongoc_server_description_t     *description,
                                               mongoc_server_description_type_t type);

#endif
