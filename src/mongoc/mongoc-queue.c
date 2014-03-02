/*
 * Copyright 2013 MongoDB, Inc.
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


#include <string.h>

#include "mongoc-queue-private.h"


void
_mongoc_queue_init (mongoc_queue_t *queue)
{
   bson_return_if_fail(queue);

   memset (queue, 0, sizeof *queue);
}


void
_mongoc_queue_push_head (mongoc_queue_t *queue,
                         void           *data)
{
   mongoc_queue_item_t *item;

   bson_return_if_fail(queue);
   bson_return_if_fail(data);

   item = bson_malloc0(sizeof *item);
   item->next = queue->head;
   item->data = data;

   queue->head = item;

   if (!queue->tail) {
      queue->tail = item;
   }
}


void
_mongoc_queue_push_tail (mongoc_queue_t *queue,
                         void           *data)
{
   mongoc_queue_item_t *item;

   bson_return_if_fail(queue);
   bson_return_if_fail(data);

   item = bson_malloc0(sizeof *item);
   item->data = data;

   if (queue->tail) {
      queue->tail->next = item;
   } else {
      queue->head = item;
   }

   queue->tail = item;
}


void *
_mongoc_queue_pop_head (mongoc_queue_t *queue)
{
   mongoc_queue_item_t *item;
   void *data = NULL;

   bson_return_val_if_fail(queue, NULL);

   if ((item = queue->head)) {
      if (!item->next) {
         queue->tail = NULL;
      }
      queue->head = item->next;
      data = item->data;
      bson_free(item);
   }

   return data;
}


uint32_t
_mongoc_queue_get_length (const mongoc_queue_t *queue)
{
   mongoc_queue_item_t *item;
   uint32_t count = 0;

   bson_return_val_if_fail(queue, 0);

   for (item = queue->head; item; item = item->next) {
      count++;
   }

   return count;
}
