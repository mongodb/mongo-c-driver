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

/* POSIX reference primitives for exercising the runtime thread backend.
 * This is not a coroutine scheduler. Mutexes, conditions, and rwlocks deliberately
 * use heap-allocated handles to catch assumptions about their representation. */
#ifndef MCOMMON_TEST_THREAD_BACKEND_H
#define MCOMMON_TEST_THREAD_BACKEND_H

#include <pthread.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/* Do not use assert for calls with side effects: tests also build with NDEBUG. */
static inline void
backend_check(int result)
{
   if (result != 0) {
      abort();
   }
}

static inline void *
backend_alloc(size_t size)
{
   void *p = malloc(size);
   if (!p) {
      abort();
   }
   return p;
}

typedef struct backend_mutex {
   pthread_mutex_t native;
   pthread_t owner;
   bool owned;
} *mcommon_thread_backend_mutex_t;

static inline void
mcommon_thread_backend_mutex_init(mcommon_thread_backend_mutex_t *mutex)
{
   *mutex = backend_alloc(sizeof **mutex);
   backend_check(pthread_mutex_init(&(*mutex)->native, NULL));
   (*mutex)->owned = false;
}

static inline void
mcommon_thread_backend_mutex_destroy(mcommon_thread_backend_mutex_t *mutex)
{
   backend_check(pthread_mutex_destroy(&(*mutex)->native));
   free(*mutex);
   *mutex = NULL;
}

static inline void
mcommon_thread_backend_mutex_lock(mcommon_thread_backend_mutex_t *mutex)
{
   backend_check(pthread_mutex_lock(&(*mutex)->native));
   (*mutex)->owner = pthread_self();
   (*mutex)->owned = true;
}

static inline void
mcommon_thread_backend_mutex_unlock(mcommon_thread_backend_mutex_t *mutex)
{
   (*mutex)->owned = false;
   backend_check(pthread_mutex_unlock(&(*mutex)->native));
}

static inline bool
mcommon_thread_backend_mutex_is_locked(mcommon_thread_backend_mutex_t *mutex)
{
   return (*mutex)->owned && pthread_equal((*mutex)->owner, pthread_self());
}

typedef pthread_rwlock_t *mcommon_thread_backend_shared_mutex_t;

static inline void
mcommon_thread_backend_shared_mutex_init(mcommon_thread_backend_shared_mutex_t *mutex)
{
   *mutex = backend_alloc(sizeof **mutex);
   backend_check(pthread_rwlock_init(*mutex, NULL));
}

static inline void
mcommon_thread_backend_shared_mutex_destroy(mcommon_thread_backend_shared_mutex_t *mutex)
{
   backend_check(pthread_rwlock_destroy(*mutex));
   free(*mutex);
   *mutex = NULL;
}

static inline void
mcommon_thread_backend_shared_mutex_lock(mcommon_thread_backend_shared_mutex_t *mutex)
{
   backend_check(pthread_rwlock_wrlock(*mutex));
}

static inline void
mcommon_thread_backend_shared_mutex_lock_shared(mcommon_thread_backend_shared_mutex_t *mutex)
{
   backend_check(pthread_rwlock_rdlock(*mutex));
}

static inline void
mcommon_thread_backend_shared_mutex_unlock(mcommon_thread_backend_shared_mutex_t *mutex)
{
   backend_check(pthread_rwlock_unlock(*mutex));
}

static inline void
mcommon_thread_backend_shared_mutex_unlock_shared(mcommon_thread_backend_shared_mutex_t *mutex)
{
   backend_check(pthread_rwlock_unlock(*mutex));
}

typedef pthread_cond_t *mcommon_thread_backend_cond_t;

static inline void
mcommon_thread_backend_cond_init(mcommon_thread_backend_cond_t *cond)
{
   *cond = backend_alloc(sizeof **cond);
   backend_check(pthread_cond_init(*cond, NULL));
}

static inline void
mcommon_thread_backend_cond_destroy(mcommon_thread_backend_cond_t *cond)
{
   backend_check(pthread_cond_destroy(*cond));
   free(*cond);
   *cond = NULL;
}

static inline int
mcommon_thread_backend_cond_wait(mcommon_thread_backend_cond_t *cond, mcommon_thread_backend_mutex_t *mutex)
{
   (*mutex)->owned = false;
   int ret = pthread_cond_wait(*cond, &(*mutex)->native);
   (*mutex)->owner = pthread_self();
   (*mutex)->owned = true;
   return ret;
}

static inline int
mcommon_thread_backend_cond_timedwait(mcommon_thread_backend_cond_t *cond,
                                      mcommon_thread_backend_mutex_t *mutex,
                                      int64_t timeout_ms)
{
   struct timeval now;
   backend_check(gettimeofday(&now, NULL));
   if (timeout_ms < 0) {
      timeout_ms = 0;
   }
   struct timespec until;
   until.tv_sec = now.tv_sec + timeout_ms / 1000;
   until.tv_nsec = now.tv_usec * 1000 + (timeout_ms % 1000) * 1000000;
   until.tv_sec += until.tv_nsec / 1000000000;
   until.tv_nsec %= 1000000000;
   (*mutex)->owned = false;
   int ret = pthread_cond_timedwait(*cond, &(*mutex)->native, &until);
   (*mutex)->owner = pthread_self();
   (*mutex)->owned = true;
   return ret;
}

static inline bool
mcommon_thread_backend_cond_is_timedout(int result)
{
   return result == ETIMEDOUT;
}

static inline void
mcommon_thread_backend_cond_signal(mcommon_thread_backend_cond_t *cond)
{
   backend_check(pthread_cond_signal(*cond));
}

static inline void
mcommon_thread_backend_cond_broadcast(mcommon_thread_backend_cond_t *cond)
{
   backend_check(pthread_cond_broadcast(*cond));
}

static inline void
mcommon_thread_backend_sleep(int64_t usec)
{
   if (usec <= 0) {
      return;
   }
   struct timespec remaining = {usec / 1000000, (usec % 1000000) * 1000};
   while (nanosleep(&remaining, &remaining) != 0) {
      if (errno != EINTR) {
         abort();
      }
   }
}

#endif
