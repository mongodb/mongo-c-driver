/*
 * Copyright 2015 MongoDB, Inc.
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

#include "mongoc-array-private.h"
#include "mongoc-thread-private.h"

#include "queue.h"


struct _queue_t {
   mongoc_array_t array;
   mongoc_cond_t      cond;
   mongoc_mutex_t     mutex;
};


queue_t *
q_new ()
{
   queue_t *q = bson_malloc (sizeof(queue_t));

   _mongoc_array_init (&q->array, sizeof(void *));
   mongoc_cond_init (&q->cond);
   mongoc_mutex_init (&q->mutex);

   return q;
}

void
q_put (queue_t *q, void *item)
{
   mongoc_mutex_lock (&q->mutex);
   _mongoc_array_append_val (&q->array, item);
   mongoc_cond_signal (&q->cond);
   mongoc_mutex_unlock (&q->mutex);
}


void *
q_get (queue_t *q, int64_t timeout_msec)
{
   void *item = NULL;
   size_t i;
   int64_t deadline;
   void **data;

   mongoc_mutex_lock (&q->mutex);
   if (timeout_msec) {
      deadline = bson_get_monotonic_time () + timeout_msec * 1000;
      while (!q->array.len && bson_get_monotonic_time () <= deadline) {
         mongoc_cond_timedwait (&q->cond, &q->mutex, timeout_msec);
      }
   } else {
      /* no deadline */
      while (!q->array.len) {
         mongoc_cond_wait (&q->cond, &q->mutex);
      }
   }

   if (q->array.len) {
      data = (void **)q->array.data;
      item = data[0];

      /* shift the queue left */
      q->array.len--;
      for (i = 0; i < q->array.len; i++) {
         data[i] = data[i + 1];
      }
   }

   mongoc_mutex_unlock (&q->mutex);

   return item;
}

void
q_destroy (queue_t *q)
{
   _mongoc_array_destroy (&q->array);
   mongoc_cond_destroy (&q->cond);
   mongoc_mutex_destroy (&q->mutex);
   bson_free (q);
}
