#include "./mongoc-ts-pool-private.h"
#include "common-thread-private.h"

#include "bson/bson.h"

typedef struct pool_node {
   struct pool_node *next;
   max_align_t data[1];
} pool_node;

typedef struct _mongoc_ts_pool {
   void (*element_dtor) (void *);
   pool_node *head;
   size_t element_size;
   size_t size;
   bson_mutex_t mtx;
} _mongoc_ts_pool;

mongoc_ts_pool
mongoc_ts_pool_new (size_t element_size, void (*dtor) (void *))
{
   mongoc_ts_pool r = bson_malloc0 (sizeof (_mongoc_ts_pool));
   r->element_dtor = dtor;
   r->head = NULL;
   r->element_size = element_size;
   r->size = 0;
   bson_mutex_init (&r->mtx);
   return r;
}

void
mongoc_ts_pool_free (mongoc_ts_pool pool)
{
   mongoc_ts_pool_clear (pool);
   bson_mutex_destroy (&pool->mtx);
   bson_free (pool);
}

void
mongoc_ts_pool_clear (mongoc_ts_pool pool)
{
   pool_node *node;
   bson_mutex_lock (&pool->mtx);
   node = pool->head;
   pool->head = NULL;
   pool->size = 0;
   bson_mutex_unlock (&pool->mtx);
   while (node) {
      pool_node *n = node;
      pool->element_dtor (n->data);
      node = n->next;
      bson_free (n);
   }
}

void *
mongoc_ts_pool_alloc_item (mongoc_ts_pool pool)
{
   pool_node *node = bson_malloc0 (sizeof (pool_node) + pool->element_size);
   return node->data;
}

void *
mongoc_ts_pool_try_pop (mongoc_ts_pool pool, bool *const did_pop)
{
   pool_node *node;
   bson_mutex_lock (&pool->mtx);
   node = pool->head;
   if (node) {
      pool->head = node->next;
      pool->size--;
   }
   bson_mutex_unlock (&pool->mtx);
   if (did_pop) {
      *did_pop = node != NULL;
   }
   return node ? node->data : NULL;
}

void
mongoc_ts_pool_push (mongoc_ts_pool pool, void *userdata)
{
   pool_node *node =
      (void *) ((uint8_t *) (userdata) -offsetof (pool_node, data));

   bson_mutex_lock (&pool->mtx);
   node->next = pool->head;
   pool->head = node;
   pool->size++;
   bson_mutex_unlock (&pool->mtx);
   return;
}

bool
mongoc_ts_pool_is_empty (mongoc_ts_pool pool)
{
   return mongoc_ts_pool_size (pool) == 0;
}

size_t
mongoc_ts_pool_size (mongoc_ts_pool pool)
{
   size_t r = 0;
   bson_mutex_lock (&pool->mtx);
   r = pool->size;
   bson_mutex_unlock (&pool->mtx);
   return r;
}

void
mongoc_ts_pool_free_item (mongoc_ts_pool pool, void *userdata)
{
   pool_node *node =
      (void *) ((uint8_t *) (userdata) -offsetof (pool_node, data));
   pool->element_dtor (node->data);
   bson_free (node);
}
