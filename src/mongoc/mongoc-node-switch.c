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


#include <bson.h>

#include "mongoc-node-switch-private.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "node_switch"

#define MONGOC_NODE_SWITCH_DEFAULT_SIZE 8

mongoc_node_switch_t *
mongoc_node_switch_new (void)
{
   mongoc_node_switch_t *ns = bson_malloc (sizeof (*ns));

   ns->nodes_allocated = MONGOC_NODE_SWITCH_DEFAULT_SIZE;
   ns->nodes = bson_malloc (sizeof (*ns->nodes) * ns->nodes_allocated);
   ns->nodes_len = 0;

   return ns;
}

void
mongoc_node_switch_add (mongoc_node_switch_t *ns,
                        uint32_t              id,
                        mongoc_stream_t      *stream)
{
   assert(id < UINT32_MAX);

   if (ns->nodes_len > 0) {
      assert(ns->nodes[ns->nodes_len - 1].id < id);
   }

   if (ns->nodes_len >= ns->nodes_allocated) {
      ns->nodes_allocated *= 2;
      bson_realloc (ns->nodes, sizeof (*ns->nodes) * ns->nodes_allocated);
   }

   ns->nodes[ns->nodes_len].id = id;
   ns->nodes[ns->nodes_len].stream = stream;

   ns->nodes_len++;
}

static int
mongoc_node_switch_id_cmp (const void *a_,
                           const void *b_)
{
   mongoc_node_t *a = (mongoc_node_t *)a_;
   mongoc_node_t *b = (mongoc_node_t *)b_;

   if (a->id == b->id) {
      return 0;
   }

   return a->id < b->id ? -1 : 1;
}

void
mongoc_node_switch_rm (mongoc_node_switch_t *ns,
                       uint32_t              id)
{
   mongoc_node_t *ptr;
   mongoc_node_t key;
   int i;

   key.id = id;

   ptr = bsearch (&key, ns->nodes, ns->nodes_len, sizeof (key),
                  mongoc_node_switch_id_cmp);

   if (ptr) {
      mongoc_stream_destroy (ptr->stream);

      i = ptr - ns->nodes;

      if (i != ns->nodes_len - 1) {
         memmove (ns->nodes + i, ns->nodes + i + 1, ns->nodes_len - (i + 1));
      }

      ns->nodes_len--;
   }
}

mongoc_stream_t *
mongoc_node_switch_get (mongoc_node_switch_t *ns,
                        uint32_t              id)
{
   mongoc_node_t *ptr;
   mongoc_node_t key;

   key.id = id;

   ptr = bsearch (&key, ns->nodes, ns->nodes_len, sizeof (key),
                  mongoc_node_switch_id_cmp);

   return ptr ? ptr->stream : NULL;
}

void
mongoc_node_switch_destroy (mongoc_node_switch_t *ns)
{
   int i;

   for (i = 0; i < ns->nodes_len; i++) {
      mongoc_stream_destroy (ns->nodes[i].stream);
   }

   bson_free (ns->nodes);
   bson_free (ns);
}
