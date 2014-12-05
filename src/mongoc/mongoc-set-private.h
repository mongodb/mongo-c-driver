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

#ifndef MONGOC_SET_PRIVATE_H
#define MONGOC_SET_PRIVATE_H

#if !defined (MONGOC_I_AM_A_DRIVER) && !defined (MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>
#include "mongoc-stream.h"

BSON_BEGIN_DECLS

typedef void (*mongoc_set_item_dtor)(void *item,
                                     void *ctx);

typedef struct
{
   uint32_t id;
   void    *item;
} mongoc_set_item_t;

typedef struct
{
   mongoc_set_item_t   *items;
   size_t               items_len;
   size_t               items_allocated;
   mongoc_set_item_dtor dtor;
   void                *dtor_ctx;
} mongoc_set_t;

mongoc_set_t *
mongoc_set_new (size_t               nitems,
                mongoc_set_item_dtor dtor,
                void                *dtor_ctx);

void
mongoc_set_add (mongoc_set_t *set,
                uint32_t      id,
                void         *item);

void
mongoc_set_rm (mongoc_set_t *set,
               uint32_t      id);

void *
mongoc_set_get (mongoc_set_t *set,
                uint32_t      id);

void
mongoc_set_destroy (mongoc_set_t *set);

BSON_END_DECLS

#endif /* MONGOC_SET_PRIVATE_H */
