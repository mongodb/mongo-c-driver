#include "mongoc-prelude.h"

#ifndef MONGOC_UTIL_H
#define MONGOC_UTIL_H

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
 */
MONGOC_EXPORT (void)
mongoc_usleep_set_impl (mongoc_usleep_func_t usleep_func, void *user_data);

MONGOC_EXPORT (void)
mongoc_usleep_default_impl (int64_t usec, void *user_data);

BSON_END_DECLS

#endif /* MONGOC_UTIL_H */
