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


#include "mongoc-counters-private.h"
#include "mongoc-client-pool.h"
#include "mongoc-queue-private.h"
#include "mongoc-thread-private.h"
#include "mongoc-cluster-private.h"
#include "mongoc-client-private.h"
#include "mongoc-trace.h"


struct _mongoc_client_pool_t
{
   mongoc_mutex_t    mutex;
   mongoc_cond_t     cond;
   mongoc_queue_t    queue;
   mongoc_uri_t     *uri;
   uint32_t          min_pool_size;
   uint32_t          max_pool_size;
   uint32_t          size;
#ifdef MONGOC_ENABLE_SSL
   bool              ssl_opts_set;
   mongoc_ssl_opt_t  ssl_opts;
#endif
};


#ifdef MONGOC_ENABLE_SSL
void
mongoc_client_pool_set_ssl_opts (mongoc_client_pool_t   *pool,
                                 const mongoc_ssl_opt_t *opts)
{
   bson_return_if_fail (pool);

   mongoc_mutex_lock (&pool->mutex);

   memset (&pool->ssl_opts, 0, sizeof pool->ssl_opts);
   pool->ssl_opts_set = false;

   if (opts) {
      memcpy (&pool->ssl_opts, opts, sizeof pool->ssl_opts);
      pool->ssl_opts_set = true;

   }

   mongoc_mutex_unlock (&pool->mutex);
}
#endif


mongoc_client_pool_t *
mongoc_client_pool_new (const mongoc_uri_t *uri)
{
   mongoc_client_pool_t *pool;
   const bson_t *b;
   bson_iter_t iter;

   ENTRY;

   bson_return_val_if_fail(uri, NULL);

   pool = bson_malloc0(sizeof *pool);
   mongoc_mutex_init(&pool->mutex);
   _mongoc_queue_init(&pool->queue);
   pool->uri = mongoc_uri_copy(uri);
   pool->min_pool_size = 0;
   pool->max_pool_size = 100;
   pool->size = 0;

   b = mongoc_uri_get_options(pool->uri);

   if (bson_iter_init_find_case(&iter, b, "minpoolsize")) {
      if (BSON_ITER_HOLDS_INT32(&iter)) {
         pool->min_pool_size = BSON_MAX(0, bson_iter_int32(&iter));
      }
   }

   if (bson_iter_init_find_case(&iter, b, "maxpoolsize")) {
      if (BSON_ITER_HOLDS_INT32(&iter)) {
         pool->max_pool_size = BSON_MAX(1, bson_iter_int32(&iter));
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
   mongoc_mutex_destroy(&pool->mutex);
   mongoc_cond_destroy(&pool->cond);
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

   mongoc_mutex_lock(&pool->mutex);

again:
   if (!(client = _mongoc_queue_pop_head(&pool->queue))) {
      if (pool->size < pool->max_pool_size) {
         client = mongoc_client_new_from_uri(pool->uri);
#ifdef MONGOC_ENABLE_SSL
         if (pool->ssl_opts_set) {
            mongoc_client_set_ssl_opts (client, &pool->ssl_opts);
         }
#endif
         pool->size++;
      } else {
         mongoc_cond_wait(&pool->cond, &pool->mutex);
         GOTO(again);
      }
   }

   mongoc_mutex_unlock(&pool->mutex);

   RETURN(client);
}


mongoc_client_t *
mongoc_client_pool_try_pop (mongoc_client_pool_t *pool)
{
   mongoc_client_t *client;

   ENTRY;

   bson_return_val_if_fail(pool, NULL);

   mongoc_mutex_lock(&pool->mutex);

   if (!(client = _mongoc_queue_pop_head(&pool->queue))) {
      if (pool->size < pool->max_pool_size) {
         client = mongoc_client_new_from_uri(pool->uri);
#ifdef MONGOC_ENABLE_SSL
         if (pool->ssl_opts_set) {
            mongoc_client_set_ssl_opts (client, &pool->ssl_opts);
         }
#endif
         pool->size++;
      }
   }

   mongoc_mutex_unlock(&pool->mutex);

   RETURN(client);
}


void
mongoc_client_pool_push (mongoc_client_pool_t *pool,
                         mongoc_client_t      *client)
{
   ENTRY;

   bson_return_if_fail(pool);
   bson_return_if_fail(client);

   if (pool->size > pool->min_pool_size) {
      mongoc_client_t *old_client;
      old_client = _mongoc_queue_pop_head (&pool->queue);
      mongoc_client_destroy (old_client);
      pool->size--;
   }

   if ((client->cluster.state == MONGOC_CLUSTER_STATE_HEALTHY) ||
       (client->cluster.state == MONGOC_CLUSTER_STATE_BORN)) {
      mongoc_mutex_lock (&pool->mutex);
      _mongoc_queue_push_tail (&pool->queue, client);
   } else if (_mongoc_cluster_reconnect (&client->cluster, NULL)) {
      mongoc_mutex_lock (&pool->mutex);
      _mongoc_queue_push_tail (&pool->queue, client);
   } else {
      mongoc_client_destroy (client);
      mongoc_mutex_lock (&pool->mutex);
      pool->size--;
   }

   mongoc_cond_signal(&pool->cond);
   mongoc_mutex_unlock(&pool->mutex);

   EXIT;
}

size_t
mongoc_client_pool_get_size (mongoc_client_pool_t *pool)
{
   size_t size = 0;

   ENTRY;

   mongoc_mutex_lock (&pool->mutex);
   size = pool->size;
   mongoc_mutex_unlock (&pool->mutex);

   RETURN (size);
}
