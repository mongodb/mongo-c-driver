#include "mongoc-prelude.h"

#ifndef MONGOC_SLEEP_H
#define MONGOC_SLEEP_H

#include <bson/bson.h>

#include "mongoc-macros.h"

BSON_BEGIN_DECLS

/**
 * mongoc_usleep_func_t:
 * @usec: Number of microseconds to sleep for.
 * @user_data: User data provided to mongoc_usleep_set_impl().
 */
typedef void (*mongoc_usleep_func_t) (int64_t usec, void *user_data);

/**
 * mongoc_usleep_set_impl:
 * @usleep_func: A function to perform microsecond sleep.
 *
 * Sets the function to be called to perform sleep.
 * Not thread-safe.
 * Providing a `usleep_func` that does not sleep (e.g. coroutine suspension) is
 * not supported. Doing so is at the user's own risk.
 */
MONGOC_EXPORT (mongoc_usleep_func_t)
mongoc_usleep_set_impl (mongoc_usleep_func_t usleep_func,
                        void *user_data,
                        void **old_user_data);

MONGOC_EXPORT (void)
mongoc_usleep_default_impl (int64_t usec, void *user_data);

BSON_END_DECLS

#endif /* MONGOC_SLEEP_H */
