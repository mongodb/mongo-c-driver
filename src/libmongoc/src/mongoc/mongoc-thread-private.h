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

#include <mongoc/mongoc-prelude.h>

#ifndef MONGOC_THREAD_PRIVATE_H
#define MONGOC_THREAD_PRIVATE_H

#include <common-thread-private.h>

#include <mongoc/mongoc-config.h>
#include <mongoc/mongoc-log.h>

#include <bson/bson.h>

#if defined(BSON_OS_UNIX)
#define mongoc_cond_t pthread_cond_t
#define mongoc_cond_broadcast_native pthread_cond_broadcast
#define mongoc_cond_init_native(_n) pthread_cond_init((_n), NULL)

#if defined(MONGOC_ENABLE_DEBUG_ASSERTIONS)
#define mongoc_cond_wait_native(cond, mutex) pthread_cond_wait(cond, &(mutex)->wrapped_mutex)
#else
#define mongoc_cond_wait_native pthread_cond_wait
#endif

#define mongoc_cond_signal_native pthread_cond_signal
static BSON_INLINE int
mongoc_cond_timedwait_native(pthread_cond_t *cond, bson_mutex_t *mutex, int64_t timeout_msec)
{
   struct timespec to;
   struct timeval tv;
   int64_t msec;

   bson_gettimeofday(&tv);

   msec = ((int64_t)tv.tv_sec * 1000) + (tv.tv_usec / 1000) + timeout_msec;

   to.tv_sec = msec / 1000;
   to.tv_nsec = (msec % 1000) * 1000 * 1000;

#if defined(MONGOC_ENABLE_DEBUG_ASSERTIONS) && defined(BSON_OS_UNIX)
   return pthread_cond_timedwait(cond, &mutex->wrapped_mutex, &to);
#else
   return pthread_cond_timedwait(cond, mutex, &to);
#endif
}
static BSON_INLINE bool
mongo_cond_ret_is_timedout_native(int ret)
{
   return ret == ETIMEDOUT;
}
#define mongoc_cond_destroy_native pthread_cond_destroy
#else
#define mongoc_cond_t CONDITION_VARIABLE
#define mongoc_cond_init_native InitializeConditionVariable
#define mongoc_cond_wait_native(_c, _m) mongoc_cond_timedwait_native((_c), (_m), INFINITE)
static BSON_INLINE int
mongoc_cond_timedwait_native(mongoc_cond_t *cond, bson_mutex_t *mutex, int64_t timeout_msec)
{
   int r;

   if (SleepConditionVariableCS(cond, mutex, (DWORD)timeout_msec)) {
      return 0;
   } else {
      r = GetLastError();

      if (r == WAIT_TIMEOUT || r == ERROR_TIMEOUT) {
         return WSAETIMEDOUT;
      } else {
         return EINVAL;
      }
   }
}
static BSON_INLINE bool
mongo_cond_ret_is_timedout_native(int ret)
{
   return ret == WSAETIMEDOUT;
}
#define mongoc_cond_signal_native WakeConditionVariable
#define mongoc_cond_broadcast_native WakeAllConditionVariable
static BSON_INLINE int
mongoc_cond_destroy_native(mongoc_cond_t *_ignored)
{
   (void)_ignored;
   return 0;
}
#endif

#define MONGOC_DISPATCH_COND_VOID(name)                                    \
   static BSON_INLINE void mongoc_##name##_dispatch(mongoc_cond_t *cond)   \
   {                                                                       \
      const bson_thread_backend_t *backend = mcommon_thread_backend_get(); \
      if (backend && backend->name) {                                      \
         backend->name(cond);                                              \
      } else {                                                             \
         (void)mongoc_##name##_native(cond);                               \
      }                                                                    \
   }

MONGOC_DISPATCH_COND_VOID(cond_init)
MONGOC_DISPATCH_COND_VOID(cond_destroy)
MONGOC_DISPATCH_COND_VOID(cond_signal)
MONGOC_DISPATCH_COND_VOID(cond_broadcast)
#undef MONGOC_DISPATCH_COND_VOID

static BSON_INLINE int
mongoc_cond_wait_dispatch(mongoc_cond_t *cond, bson_mutex_t *mutex)
{
   const bson_thread_backend_t *backend = mcommon_thread_backend_get();
   return backend && backend->cond_wait ? backend->cond_wait(cond, mutex) : mongoc_cond_wait_native(cond, mutex);
}

static BSON_INLINE int
mongoc_cond_timedwait_dispatch(mongoc_cond_t *cond, bson_mutex_t *mutex, int64_t timeout_ms)
{
   const bson_thread_backend_t *backend = mcommon_thread_backend_get();
   return backend && backend->cond_timedwait ? backend->cond_timedwait(cond, mutex, timeout_ms)
                                             : mongoc_cond_timedwait_native(cond, mutex, timeout_ms);
}

static BSON_INLINE bool
mongo_cond_ret_is_timedout_dispatch(int result)
{
   const bson_thread_backend_t *backend = mcommon_thread_backend_get();
   return backend && backend->cond_is_timedout ? backend->cond_is_timedout(result)
                                               : mongo_cond_ret_is_timedout_native(result);
}

#define mongoc_cond_init mongoc_cond_init_dispatch
#define mongoc_cond_destroy mongoc_cond_destroy_dispatch
#define mongoc_cond_wait mongoc_cond_wait_dispatch
#define mongoc_cond_timedwait mongoc_cond_timedwait_dispatch
#define mongoc_cond_signal mongoc_cond_signal_dispatch
#define mongoc_cond_broadcast mongoc_cond_broadcast_dispatch
#define mongo_cond_ret_is_timedout mongo_cond_ret_is_timedout_dispatch


#endif /* MONGOC_THREAD_PRIVATE_H */
