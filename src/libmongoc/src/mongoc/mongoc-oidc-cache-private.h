/*
 * Copyright 2009-present MongoDB, Inc.
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

#ifndef MONGOC_OIDC_CACHE_PRIVATE_H
#define MONGOC_OIDC_CACHE_PRIVATE_H

#include <mongoc/mongoc-oidc-callback.h>
#include <mongoc/mongoc-sleep.h>

// mongoc_oidc_cache_t implements the OIDC spec "Client Cache".
// Stores the OIDC callback, cache, and lock.
// Expected to be shared among all clients in a pool.
typedef struct mongoc_oidc_cache_t mongoc_oidc_cache_t;

mongoc_oidc_cache_t *
mongoc_oidc_cache_new(void);

// mongoc_oidc_cache_set_callback sets the token callback.
// Not thread safe. Call before any authentication can occur.
void
mongoc_oidc_cache_set_callback(mongoc_oidc_cache_t *cache, const mongoc_oidc_callback_t *cb);

// mongoc_oidc_cache_get_callback gets the token callback.
const mongoc_oidc_callback_t *
mongoc_oidc_cache_get_callback(const mongoc_oidc_cache_t *cache);

// mongoc_oidc_cache_set_usleep_fn sets a custom sleep function.
// Not thread safe. Call before any authentication can occur.
void
mongoc_oidc_cache_set_usleep_fn(mongoc_oidc_cache_t *cache, mongoc_usleep_func_t usleep_fn, void *usleep_data);

// mongoc_oidc_cache_get_token returns a token or NULL on error. Thread safe.
// Sets *found_in_cache to indicate if the returned token came from the cache or callback.
// Calls sleep if needed to enforce 100ms delay between calls to the callback.
char *
mongoc_oidc_cache_get_token(mongoc_oidc_cache_t *cache, bool *found_in_cache, bson_error_t *error);

// mongoc_oidc_cache_get_cached_token returns a cached token or NULL if none is cached. Thread safe.
char *
mongoc_oidc_cache_get_cached_token(const mongoc_oidc_cache_t *cache);

// mongoc_oidc_cache_set_cached_token overwrites the cached token. Useful for tests. Thread safe.
void
mongoc_oidc_cache_set_cached_token(mongoc_oidc_cache_t *cache, const char *token);

// mongoc_oidc_cache_invalidate_token invalidates the cached token if it matches `token`. Thread safe.
void
mongoc_oidc_cache_invalidate_token(mongoc_oidc_cache_t *cache, const char *token);

void
mongoc_oidc_cache_destroy(mongoc_oidc_cache_t *);

#endif // MONGOC_OIDC_CACHE_PRIVATE_H
