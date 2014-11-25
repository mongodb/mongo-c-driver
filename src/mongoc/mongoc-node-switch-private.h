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

#ifndef MONGOC_NODE_SWITCH_PRIVATE_H
#define MONGOC_NODE_SWITCH_PRIVATE_H

#if !defined (MONGOC_I_AM_A_DRIVER) && !defined (MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>
#include "mongoc-stream.h"

BSON_BEGIN_DECLS

typedef struct
{
   uint32_t         id;
   mongoc_stream_t *stream;
} mongoc_node_t;

typedef struct
{
   mongoc_node_t *nodes;
   size_t         nodes_len;
   size_t         nodes_allocated;
} mongoc_node_switch_t;

mongoc_node_switch_t *
mongoc_node_switch_new (void);

void
mongoc_node_switch_add (mongoc_node_switch_t *ns,
                        uint32_t              id,
                        mongoc_stream_t      *stream);

void
mongoc_node_switch_rm (mongoc_node_switch_t *ns,
                       uint32_t              id);

mongoc_stream_t *
mongoc_node_switch_get (mongoc_node_switch_t *ns,
                        uint32_t              id);

void
mongoc_node_switch_destroy (mongoc_node_switch_t *ns);

BSON_END_DECLS

#endif /* MONGOC_NODE_SWITCH_PRIVATE_H */
