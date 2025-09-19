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

#include <common-thread-private.h>
#include <mongoc/mongoc-error-private.h>
#include <mongoc/mongoc-oidc-cache-private.h>
#include <mongoc/mongoc-oidc-callback-private.h>

#include <mlib/duration.h>
#include <mlib/time_point.h>

#define SET_ERROR(...) _mongoc_set_error(error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, __VA_ARGS__)

struct mongoc_oidc_cache_t {
   // callback is owned. NULL if unset. Not guarded by lock. Set before requesting tokens.
   mongoc_oidc_callback_t *callback;

   // usleep_fn is used to sleep between calls to the callback. Not guarded by lock. Set before requesting tokens.
   mongoc_usleep_func_t usleep_fn;
   void *usleep_data;

   // lock is used to prevent concurrent calls to callback. Guards access to token, last_called, and ever_called.
   bson_mutex_t lock;

   // token is a cached OIDC access token.
   char *token;

   // last_call tracks the time just after the last call to the callback.
   mlib_time_point last_called;

   // ever_called is set to true after the first call to the callback.
   bool ever_called;
};

mongoc_oidc_cache_t *
mongoc_oidc_cache_new(void)
{
   mongoc_oidc_cache_t *oidc = bson_malloc0(sizeof(mongoc_oidc_cache_t));
   oidc->usleep_fn = mongoc_usleep_default_impl;
   bson_mutex_init(&oidc->lock);
   return oidc;
}

void
mongoc_oidc_cache_set_callback(mongoc_oidc_cache_t *cache, const mongoc_oidc_callback_t *cb)
{
   BSON_ASSERT_PARAM(cache);
   BSON_OPTIONAL_PARAM(cb);

   if (cache->callback) {
      mongoc_oidc_callback_destroy(cache->callback);
   }
   cache->callback = cb ? mongoc_oidc_callback_copy(cb) : NULL;
}

const mongoc_oidc_callback_t *
mongoc_oidc_cache_get_callback(mongoc_oidc_cache_t *cache)
{
   BSON_ASSERT_PARAM(cache);

   return cache->callback;
}

void
mongoc_oidc_cache_set_usleep_fn(mongoc_oidc_cache_t *cache, mongoc_usleep_func_t usleep_fn, void *usleep_data)
{
   BSON_ASSERT_PARAM(cache);
   BSON_OPTIONAL_PARAM(usleep_fn);
   BSON_OPTIONAL_PARAM(usleep_data);

   cache->usleep_fn = usleep_fn ? usleep_fn : mongoc_usleep_default_impl;
   cache->usleep_data = usleep_data;
}

void
mongoc_oidc_cache_destroy(mongoc_oidc_cache_t *cache)
{
   if (!cache) {
      return;
   }
   bson_free(cache->token);
   bson_mutex_destroy(&cache->lock);
   mongoc_oidc_callback_destroy(cache->callback);
   bson_free(cache);
}

char *
mongoc_oidc_cache_get_cached_token(mongoc_oidc_cache_t *cache)
{
   BSON_ASSERT_PARAM(cache);

   bson_mutex_lock(&cache->lock);
   const char *token = cache->token;
   bson_mutex_unlock(&cache->lock);
   return token ? bson_strdup(token) : NULL;
}

void
mongoc_oidc_cache_set_cached_token(mongoc_oidc_cache_t *cache, const char *token)
{
   BSON_ASSERT_PARAM(cache);
   BSON_OPTIONAL_PARAM(token);

   bson_mutex_lock(&cache->lock);

   if (cache->token) {
      bson_free(cache->token);
      cache->token = NULL;
   }

   cache->token = token ? bson_strdup(token) : NULL;
   bson_mutex_unlock(&cache->lock);
}

char *
mongoc_oidc_cache_get_token(mongoc_oidc_cache_t *cache, bool *is_cache, bson_error_t *error)
{
   BSON_ASSERT_PARAM(cache);
   BSON_ASSERT_PARAM(is_cache);
   BSON_OPTIONAL_PARAM(error);

   char *token = NULL;
   mongoc_oidc_credential_t *cred = NULL;

   *is_cache = false;

   if (!cache->callback) {
      SET_ERROR("MONGODB-OIDC requested, but no callback set");
      return NULL;
   }

   bson_mutex_lock(&cache->lock);

   if (NULL != cache->token) {
      // Access token is cached.
      token = bson_strdup(cache->token);
      *is_cache = true;
      goto unlock_and_return;
   }

   mongoc_oidc_callback_params_t *params = mongoc_oidc_callback_params_new();
   mongoc_oidc_callback_params_set_user_data(params, mongoc_oidc_callback_get_user_data(cache->callback));
   // From spec: "If CSOT is not applied, then the driver MUST use 1 minute as the timeout."
   // The timeout parameter (when set) is meant to be directly compared against bson_get_monotonic_time(). It is a
   // time point, not a duration.
   mongoc_oidc_callback_params_set_timeout(params, bson_get_monotonic_time() + 60 * 1000 * 1000);

   // From spec: "Wait until it has been at least 100ms since the last callback invocation"
   if (cache->ever_called) {
      mlib_duration since_last_call = mlib_time_difference(mlib_now(), cache->last_called);
      if (mlib_duration_cmp(since_last_call, <, (100, ms))) {
         mlib_duration to_sleep = mlib_duration((100, ms), minus, since_last_call);
         cache->usleep_fn(mlib_microseconds_count(to_sleep), cache->usleep_data);
      }
   }

   // Call callback:
   cred = mongoc_oidc_callback_get_fn(cache->callback)(params);

   cache->last_called = mlib_now();
   cache->ever_called = true;
   mongoc_oidc_callback_params_destroy(params);

   if (!cred) {
      SET_ERROR("MONGODB-OIDC callback failed");
      goto unlock_and_return;
   }

   token = bson_strdup(mongoc_oidc_credential_get_access_token(cred));
   cache->token = bson_strdup(token); // Cache a copy.

unlock_and_return:
   bson_mutex_unlock(&cache->lock);
   mongoc_oidc_credential_destroy(cred);
   return token;
}

void
mongoc_oidc_cache_invalidate_cached_token(mongoc_oidc_cache_t *cache, const char *token)
{
   BSON_ASSERT_PARAM(cache);
   BSON_ASSERT_PARAM(token);

   bson_mutex_lock(&cache->lock);

   if (cache->token && 0 == strcmp(cache->token, token)) {
      bson_free(cache->token);
      cache->token = NULL;
   }

   bson_mutex_unlock(&cache->lock);
}
