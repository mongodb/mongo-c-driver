/*
 * Copyright 2013 10gen Inc.
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


#include "mongoc-counters-private.h"
#include "mongoc-client-pool.h"
#include "mongoc-queue-private.h"
#include "mongoc-trace.h"


struct _mongoc_client_pool_t
{
   bson_mutex_t    mutex;
   bson_cond_t     cond;
   mongoc_queue_t  queue;
   mongoc_uri_t   *uri;
   bson_uint32_t   min_pool_size;
   bson_uint32_t   max_pool_size;
   bson_uint32_t   size;
};


mongoc_client_pool_t *
mongoc_client_pool_new (const mongoc_uri_t *uri)
{
   mongoc_client_pool_t *pool;
   const bson_t *b;
   bson_iter_t iter;

   ENTRY;

   bson_return_val_if_fail(uri, NULL);

   pool = bson_malloc0(sizeof *pool);
   bson_mutex_init(&pool->mutex, NULL);
   _mongoc_queue_init(&pool->queue);
   pool->uri = mongoc_uri_copy(uri);
   pool->min_pool_size = 0;
   pool->max_pool_size = 100;
   pool->size = 0;

   b = mongoc_uri_get_options(pool->uri);

   if (bson_iter_init_find_case(&iter, b, "minpoolsize")) {
      if (BSON_ITER_HOLDS_INT32(&iter)) {
         pool->min_pool_size = MAX(0, bson_iter_int32(&iter));
      }
   }

   if (bson_iter_init_find_case(&iter, b, "maxpoolsize")) {
      if (BSON_ITER_HOLDS_INT32(&iter)) {
         pool->max_pool_size = MAX(1, bson_iter_int32(&iter));
      }
   }

   mongoc_counter_client_pools_active_inc();

   RETURN(pool);
}


void
mongoc_client_pool_destroy (mongoc_client_pool_t *pool)
{
   mongoc_client_t *client;

   ENTRY;

   bson_return_if_fail(pool);

   while ((client = _mongoc_queue_pop_head(&pool->queue))) {
      mongoc_client_destroy(client);
   }

   mongoc_uri_destroy(pool->uri);
   bson_mutex_destroy(&pool->mutex);
   bson_cond_destroy(&pool->cond);
   bson_free(pool);

   mongoc_counter_client_pools_active_dec();
   mongoc_counter_client_pools_disposed_inc();

   EXIT;
}


mongoc_client_t *
mongoc_client_pool_pop (mongoc_client_pool_t *pool)
{
   mongoc_client_t *client;

   ENTRY;

   bson_return_val_if_fail(pool, NULL);

   bson_mutex_lock(&pool->mutex);

again:
   if (!(client = _mongoc_queue_pop_head(&pool->queue))) {
      if (pool->size < pool->max_pool_size) {
         client = mongoc_client_new_from_uri(pool->uri);
         pool->size++;
      } else {
         bson_cond_wait(&pool->cond, &pool->mutex);
         GOTO(again);
      }
   }

   bson_mutex_unlock(&pool->mutex);

   RETURN(client);
}


mongoc_client_t *
mongoc_client_pool_try_pop (mongoc_client_pool_t *pool)
{
   mongoc_client_t *client;

   ENTRY;

   bson_return_val_if_fail(pool, NULL);

   bson_mutex_lock(&pool->mutex);

   if (!(client = _mongoc_queue_pop_head(&pool->queue))) {
      if (pool->size < pool->max_pool_size) {
         client = mongoc_client_new_from_uri(pool->uri);
         pool->size++;
      }
   }

   bson_mutex_unlock(&pool->mutex);

   RETURN(client);
}


void
mongoc_client_pool_push (mongoc_client_pool_t *pool,
                         mongoc_client_t      *client)
{
   ENTRY;

   bson_return_if_fail(pool);
   bson_return_if_fail(client);

   /*
    * TODO: Shutdown old client connections.
    */

   /*
    * TODO: We should try to make a client healthy again if it
    *       is unhealthy since this is typically where a thread
    *       is done with its work.
    */

   bson_mutex_lock(&pool->mutex);
   _mongoc_queue_push_head(&pool->queue, client);
   bson_cond_signal(&pool->cond);
   bson_mutex_unlock(&pool->mutex);

   EXIT;
}
