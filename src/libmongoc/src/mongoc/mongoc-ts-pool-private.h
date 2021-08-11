#ifndef MONGOC_TS_POOL_PRIVATE_H
#define MONGOC_TS_POOL_PRIVATE_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @brief A thread-safe pool.
 *
 * Items are allocated using `mongo_ts_pool_alloc_item` and should be deleted
 * with `mongo_ts_pool_free_item`. Items are added to the pool with
 * `mongo_ts_pool_push` and taken from the pool using `mongo_ts_pool_try_pop`.
 */
typedef struct _mongoc_ts_pool *mongoc_ts_pool;

/**
 * @brief Create a new thread-safe pool
 *
 * @param element_size The size of the queue elements
 * @param element_dtor A destructor function to run when an item is removed
 */
extern mongoc_ts_pool
mongoc_ts_pool_new (size_t element_size, void (*element_dtor) (void *));

/**
 * @brief Destroy a pool of objects previously created with `mongo_ts_pool_new`
 *
 * Any objects remaining in the pool will also be destroyed.
 */
extern void mongoc_ts_pool_free (mongoc_ts_pool);

/**
 * @brief Attempt to pop an object from the pool.
 *
 * @param pool The pool of objects.
 * @param did_pop Set to `true` if an item was popped, otherwise `false`.
 *                May be NULL.
 * @return void* Pointer to an object previously passed to `mongo_ts_pool_push`,
 *               or NULL if the pool is empty.
 */
extern void *
mongoc_ts_pool_try_pop (mongoc_ts_pool pool, bool *did_pop);

/**
 * @brief Add the given item to the pool.
 *
 * @param pool The pool to insert into
 * @param userdata A pointer returned from a call to `mongo_ts_pool_try_pop` or
 * `mongo_ts_pool_alloc_item`.
 */
extern void
mongoc_ts_pool_push (mongoc_ts_pool pool, void *userdata);

/**
 * @brief Obtain the number of elements in the pool.
 *
 * This shouldn't be relied on, as the result can change atomically. Should be
 * used for diagnostic purposes only.
 */
extern size_t
mongoc_ts_pool_size (mongoc_ts_pool pool);

/**
 * @brief Determine whether the pool is empty.
 *
 * This shouldn't be relied on, as the result can change atomically. Should be
 * used for diagnostic purposes only.
 */
extern bool
mongoc_ts_pool_is_empty (mongoc_ts_pool pool);

/**
 * @brief Destroy all items and empty the given pool
 */
extern void
mongoc_ts_pool_clear (mongoc_ts_pool pool);

/**
 * @brief Allocate a buffer of data suitable for the given pool.
 *
 * @returns A pointer to uninitialized memory of the element size of the pool.
 * The memory MUST be initialized before the returned pointer is given to
 * `mongo_ts_pool_free_item` or `mongo_ts_pool_push`.
 */
extern void *
mongoc_ts_pool_alloc_item (mongoc_ts_pool pool);

/**
 * @brief Destroy an item that was created with `mongo_ts_pool_alloc_item`.
 *
 * @param item A pointer returned by `mongo_ts_pool_alloc_item` or
 * `mongo_ts_pool_try_pop`
 */
extern void
mongoc_ts_pool_free_item (mongoc_ts_pool pool, void *item);

#endif /* MONGOC_TS_POOL_PRIVATE_H */
