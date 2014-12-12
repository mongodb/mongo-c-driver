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

typedef enum
   {
      MONGOC_SERVER_UNKNOWN,
      MONGOC_SERVER_STANDALONE,
      MONGOC_SERVER_MONGOS,
      MONGOC_SERVER_POSSIBLE_PRIMARY,
      MONGOC_SERVER_RS_PRIMARY,
      MONGOC_SERVER_RS_SECONDARY,
      MONGOC_SERVER_RS_ARBITER,
      MONGOC_SERVER_RS_OTHER,
      MONGOC_SERVER_RS_GHOST,
      MONGOC_SERVER_DESCRIPTION_TYPES,
   } mongoc_server_description_type_t;

typedef struct _mongoc_server_description_t mongoc_server_description_t;

struct _mongoc_server_description_t
{
   uint32_t                         id;
   mongoc_host_list_t               host;
   int64_t                          round_trip_time;
   bson_t                           last_is_master;

   /* The following fields are filled from the last_is_master and are zeroed on
    * parse.  So order matters here.  DON'T move set_name */
   const char                      *set_name;
   const char                      *connection_address;
   char                            *error; // TODO: what type should this be?
   mongoc_server_description_type_t type;
   int32_t                          min_wire_version;
   int32_t                          max_wire_version;

   bson_t                           hosts;
   bson_t                           passives;
   bson_t                           arbiters;

   bson_t                           tags;
   const char                      *current_primary;
   int32_t                          max_write_batch_size;
};

void
_mongoc_server_description_init (mongoc_server_description_t *description,
                                 const char                  *address,
                                 uint32_t                     id);

void
_mongoc_server_description_destroy (mongoc_server_description_t *description);

bool
_mongoc_server_description_has_rs_member (mongoc_server_description_t *description,
                                          const char                  *address);

void
_mongoc_server_description_set_state (mongoc_server_description_t     *description,
                                      mongoc_server_description_type_t type);

void
_mongoc_server_description_update_rtt (mongoc_server_description_t *description,
                                       int64_t                      new_time);

mongoc_server_description_t *
_mongoc_server_description_new_copy (const mongoc_server_description_t *description);

void
_mongoc_server_description_handle_ismaster (
   mongoc_server_description_t   *sd,
   const bson_t                  *reply,
   int64_t                        rtt_msec,
   bson_error_t                  *error);

#endif
