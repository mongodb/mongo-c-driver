/*
 * Copyright 2013 MongoDB Inc.
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

#ifndef MONGOC_THREAD_PRIVATE_H
#define MONGOC_THREAD_PRIVATE_H

#if !defined(MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>

#include "common-thread-private.h"
#include "mongoc-config.h"
#include "mongoc-log.h"

/* rename symbols from common-thread-private.h */
#define MONGOC_ONCE_FUN BSON_ONCE_FUN
#define MONGOC_ONCE_INIT BSON_ONCE_INIT
#define MONGOC_ONCE_RETURN BSON_ONCE_RETURN
#define mongoc_mutex_destroy bson_mutex_destroy
#define mongoc_mutex_init bson_mutex_init
#define mongoc_mutex_lock bson_mutex_lock
#define mongoc_mutex_t bson_mutex_t
#define mongoc_mutex_unlock bson_mutex_unlock
#define mongoc_once bson_once
#define mongoc_once_t bson_once_t
#define mongoc_thread_create bson_thread_create
#define mongoc_thread_join bson_thread_join
#define mongoc_thread_t bson_thread_t

#if defined(BSON_OS_UNIX)
#include <pthread.h>
#define mongoc_cond_t pthread_cond_t
#define mongoc_cond_broadcast pthread_cond_broadcast
#define mongoc_cond_init(_n) pthread_cond_init ((_n), NULL)
#define mongoc_cond_wait pthread_cond_wait
#define mongoc_cond_signal pthread_cond_signal
static BSON_INLINE int
mongoc_cond_timedwait (pthread_cond_t *cond,
                       pthread_mutex_t *mutex,
                       int64_t timeout_msec)
{
   struct timespec to;
   struct timeval tv;
   int64_t msec;

   bson_gettimeofday (&tv);

   msec = ((int64_t) tv.tv_sec * 1000) + (tv.tv_usec / 1000) + timeout_msec;

   to.tv_sec = msec / 1000;
   to.tv_nsec = (msec % 1000) * 1000 * 1000;

   return pthread_cond_timedwait (cond, mutex, &to);
}
#define mongoc_cond_destroy pthread_cond_destroy
#else
#define mongoc_cond_t CONDITION_VARIABLE
#define mongoc_cond_init InitializeConditionVariable
#define mongoc_cond_wait(_c, _m) mongoc_cond_timedwait ((_c), (_m), INFINITE)
static BSON_INLINE int
mongoc_cond_timedwait (mongoc_cond_t *cond,
                       mongoc_mutex_t *mutex,
                       int64_t timeout_msec)
{
   int r;

   if (SleepConditionVariableCS (cond, mutex, (DWORD) timeout_msec)) {
      return 0;
   } else {
      r = GetLastError ();

      if (r == WAIT_TIMEOUT || r == ERROR_TIMEOUT) {
         return WSAETIMEDOUT;
      } else {
         return EINVAL;
      }
   }
}
#define mongoc_cond_signal WakeConditionVariable
#define mongoc_cond_broadcast WakeAllConditionVariable
static BSON_INLINE int
mongoc_cond_destroy (mongoc_cond_t *_ignored)
{
   return 0;
}
#endif


#endif /* MONGOC_THREAD_PRIVATE_H */
