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

#include <common-prelude.h>

#include <common-macros-private.h>

#include <common-config.h>

#ifndef MONGO_C_DRIVER_COMMON_THREAD_PRIVATE_H
#define MONGO_C_DRIVER_COMMON_THREAD_PRIVATE_H

#include <bson/bson-thread-backend.h>
#include <bson/compat.h>
#include <bson/config.h>
#include <bson/macros.h>

BSON_BEGIN_DECLS

#define mcommon_thread_backend_get COMMON_NAME(thread_backend_get)
#define mcommon_thread_backend_set COMMON_NAME(thread_backend_set)
const bson_thread_backend_t *
mcommon_thread_backend_get(void);
bool
mcommon_thread_backend_set(const bson_thread_backend_t *backend);

#define mcommon_thread_create COMMON_NAME(thread_create)
#define mcommon_thread_join COMMON_NAME(thread_join)

#if defined(BSON_OS_UNIX)
#include <pthread.h>

#define BSON_ONCE_FUN(n) void n(void)
#define BSON_ONCE_RETURN return
#define BSON_ONCE_INIT PTHREAD_ONCE_INIT
#define bson_once_native(o, c)                  \
   do {                                         \
      BSON_ASSERT(pthread_once((o), (c)) == 0); \
   } while (0)
#define bson_once_t pthread_once_t
typedef void *bson_thread_t;
#define BSON_THREAD_FUN(_function_name, _arg_name) void *(_function_name)(void *(_arg_name))
#define BSON_THREAD_FUN_TYPE(_function_name) void *(*(_function_name))(void *)
#define BSON_THREAD_RETURN return NULL

/* this macro can be defined as a build configuration option
 * with -DENABLE_DEBUG_ASSERTIONS=ON.  its purpose is to allow for functions
 * that require a mutex to be locked on entry to assert that the mutex
 * is actually locked.
 * this can prevent bugs where a caller forgets to lock the mutex. */

#ifndef MONGOC_ENABLE_DEBUG_ASSERTIONS

#define bson_mutex_destroy_native(m)                \
   do {                                             \
      BSON_ASSERT(pthread_mutex_destroy((m)) == 0); \
   } while (0)

#define bson_mutex_init_native(_n)                      \
   do {                                                 \
      BSON_ASSERT(pthread_mutex_init((_n), NULL) == 0); \
   } while (0)

#define bson_mutex_lock_native(m)                \
   do {                                          \
      BSON_ASSERT(pthread_mutex_lock((m)) == 0); \
   } while (0)

#define bson_mutex_t pthread_mutex_t

#define bson_mutex_unlock_native(m)                \
   do {                                            \
      BSON_ASSERT(pthread_mutex_unlock((m)) == 0); \
   } while (0)

#else
#include <mlib/config.h>

typedef struct {
   pthread_t lock_owner;
   pthread_mutex_t wrapped_mutex;
   bool valid_tid;
} bson_mutex_t;

#define bson_mutex_destroy_native(mutex)                                \
   do {                                                                 \
      BSON_ASSERT(pthread_mutex_destroy(&(mutex)->wrapped_mutex) == 0); \
   } while (0);

#define bson_mutex_init_native(mutex)                                      \
   do {                                                                    \
      BSON_ASSERT(pthread_mutex_init(&(mutex)->wrapped_mutex, NULL) == 0); \
      (mutex)->valid_tid = false;                                          \
   } while (0);

#define bson_mutex_lock_native(mutex)                                \
   do {                                                              \
      BSON_ASSERT(pthread_mutex_lock(&(mutex)->wrapped_mutex) == 0); \
      (mutex)->lock_owner = pthread_self();                          \
      (mutex)->valid_tid = true;                                     \
   } while (0);

#define bson_mutex_unlock_native(mutex)                                \
   do {                                                                \
      (mutex)->valid_tid = false;                                      \
      BSON_ASSERT(pthread_mutex_unlock(&(mutex)->wrapped_mutex) == 0); \
   } while (0);

#endif

#else
#include <process.h>
#define BSON_ONCE_FUN(n) void n(void)
#define BSON_ONCE_INIT INIT_ONCE_STATIC_INIT
#define BSON_ONCE_RETURN return
#define bson_mutex_destroy_native DeleteCriticalSection
#define bson_mutex_init_native InitializeCriticalSection
#define bson_mutex_lock_native EnterCriticalSection
#define bson_mutex_t CRITICAL_SECTION
#define bson_mutex_unlock_native LeaveCriticalSection
typedef struct {
   void (*callback)(void);
} mcommon_once_callback_t;
static BSON_INLINE BOOL CALLBACK
mcommon_once_execute(PINIT_ONCE once, PVOID parameter, PVOID *context)
{
   (void)once;
   (void)context;
   ((mcommon_once_callback_t *)parameter)->callback();
   return TRUE;
}
static BSON_INLINE void
bson_once_native(INIT_ONCE *once, void (*callback)(void))
{
   mcommon_once_callback_t param = {callback};
   BSON_ASSERT(InitOnceExecuteOnce(once, mcommon_once_execute, &param, NULL));
}
#define bson_once_t INIT_ONCE
#define bson_thread_t HANDLE
#define BSON_THREAD_FUN(_function_name, _arg_name) unsigned(__stdcall _function_name)(void *(_arg_name))
#define BSON_THREAD_FUN_TYPE(_function_name) unsigned(__stdcall * _function_name)(void *)
#define BSON_THREAD_RETURN return 0
#endif

/* Functions that require definitions get the common prefix (_mongoc for
 * libmongoc or _bson for libbson) to avoid duplicate symbols when linking both
 * libbson and libmongoc statically. */
int
mcommon_thread_join(bson_thread_t thread);
// mcommon_thread_create returns 0 on success. Returns a non-zero error code on
// error. Callers may use `bson_strerror_r` to get an error message from the
// returned error code.
int
mcommon_thread_create(bson_thread_t *thread, BSON_THREAD_FUN_TYPE(func), void *arg);

#if defined(MONGOC_ENABLE_DEBUG_ASSERTIONS) && defined(BSON_OS_UNIX)
#define mcommon_mutex_is_locked COMMON_NAME(mutex_is_locked)
bool
mcommon_mutex_is_locked(bson_mutex_t *mutex);
#endif

/**
 * @brief A shared mutex (a read-write lock)
 *
 * A shared mutex can be locked in 'shared' mode or 'exclusive' mode. Only one
 * thread may hold exclusive mode at a time. Any number of threads may hold
 * the lock in shared mode simultaneously. No thread can hold in exclusive mode
 * while another thread holds in shared mode, and vice-versa.
 */
typedef struct bson_shared_mutex_t {
   BSON_IF_WINDOWS(SRWLOCK native;)
   BSON_IF_POSIX(pthread_rwlock_t native;)
} bson_shared_mutex_t;

static BSON_INLINE void
bson_shared_mutex_init_native(bson_shared_mutex_t *mtx)
{
   BSON_IF_WINDOWS(InitializeSRWLock(&mtx->native));
   BSON_IF_POSIX(BSON_ASSERT(pthread_rwlock_init(&mtx->native, NULL) == 0);)
}

static BSON_INLINE void
bson_shared_mutex_destroy_native(bson_shared_mutex_t *mtx)
{
   BSON_IF_WINDOWS((void)mtx;)
   BSON_IF_POSIX(BSON_ASSERT(pthread_rwlock_destroy(&mtx->native) == 0);)
}

static BSON_INLINE void
bson_shared_mutex_lock_shared_native(bson_shared_mutex_t *mtx)
{
   BSON_IF_WINDOWS(AcquireSRWLockShared(&mtx->native);)
   BSON_IF_POSIX(BSON_ASSERT(pthread_rwlock_rdlock(&mtx->native) == 0);)
}

static BSON_INLINE void
bson_shared_mutex_lock_native(bson_shared_mutex_t *mtx)
{
   BSON_IF_WINDOWS(AcquireSRWLockExclusive(&mtx->native);)
   BSON_IF_POSIX(BSON_ASSERT(pthread_rwlock_wrlock(&mtx->native) == 0);)
}

static BSON_INLINE void
bson_shared_mutex_unlock_native(bson_shared_mutex_t *mtx)
{
   BSON_IF_WINDOWS(ReleaseSRWLockExclusive(&mtx->native);)
   BSON_IF_POSIX(BSON_ASSERT(pthread_rwlock_unlock(&mtx->native) == 0);)
}

static BSON_INLINE void
bson_shared_mutex_unlock_shared_native(bson_shared_mutex_t *mtx)
{
   BSON_IF_WINDOWS(ReleaseSRWLockShared(&mtx->native);)
   BSON_IF_POSIX(BSON_ASSERT(pthread_rwlock_unlock(&mtx->native) == 0);)
}

static BSON_INLINE void
mcommon_once_dispatch(bson_once_t *once, void (*callback)(void))
{
   const bson_thread_backend_t *backend = mcommon_thread_backend_get();
   if (backend && backend->once) {
      backend->once(once, callback);
   } else {
      bson_once_native(once, callback);
   }
}

#define bson_once mcommon_once_dispatch

#define MCOMMON_DISPATCH_MUTEX(name)                                       \
   static BSON_INLINE void mcommon_##name##_dispatch(bson_mutex_t *m)      \
   {                                                                       \
      const bson_thread_backend_t *backend = mcommon_thread_backend_get(); \
      if (backend && backend->name) {                                      \
         backend->name(m);                                                 \
      } else {                                                             \
         bson_##name##_native(m);                                          \
      }                                                                    \
   }

MCOMMON_DISPATCH_MUTEX(mutex_init)
MCOMMON_DISPATCH_MUTEX(mutex_destroy)
MCOMMON_DISPATCH_MUTEX(mutex_lock)
MCOMMON_DISPATCH_MUTEX(mutex_unlock)
#undef MCOMMON_DISPATCH_MUTEX

#define bson_mutex_init mcommon_mutex_init_dispatch
#define bson_mutex_destroy mcommon_mutex_destroy_dispatch
#define bson_mutex_lock mcommon_mutex_lock_dispatch
#define bson_mutex_unlock mcommon_mutex_unlock_dispatch

#define MCOMMON_DISPATCH_SHARED(name)                                        \
   static BSON_INLINE void mcommon_##name##_dispatch(bson_shared_mutex_t *m) \
   {                                                                         \
      const bson_thread_backend_t *backend = mcommon_thread_backend_get();   \
      if (backend && backend->name) {                                        \
         backend->name(m);                                                   \
      } else {                                                               \
         bson_##name##_native(m);                                            \
      }                                                                      \
   }

MCOMMON_DISPATCH_SHARED(shared_mutex_init)
MCOMMON_DISPATCH_SHARED(shared_mutex_destroy)
MCOMMON_DISPATCH_SHARED(shared_mutex_lock)
MCOMMON_DISPATCH_SHARED(shared_mutex_unlock)
MCOMMON_DISPATCH_SHARED(shared_mutex_lock_shared)
MCOMMON_DISPATCH_SHARED(shared_mutex_unlock_shared)
#undef MCOMMON_DISPATCH_SHARED

#define bson_shared_mutex_init mcommon_shared_mutex_init_dispatch
#define bson_shared_mutex_destroy mcommon_shared_mutex_destroy_dispatch
#define bson_shared_mutex_lock mcommon_shared_mutex_lock_dispatch
#define bson_shared_mutex_unlock mcommon_shared_mutex_unlock_dispatch
#define bson_shared_mutex_lock_shared mcommon_shared_mutex_lock_shared_dispatch
#define bson_shared_mutex_unlock_shared mcommon_shared_mutex_unlock_shared_dispatch

BSON_END_DECLS

#include <mlib/time_point.h>

static BSON_INLINE void
mcommon_sleep_for(mlib_duration duration)
{
   const bson_thread_backend_t *backend = mcommon_thread_backend_get();
   if (backend && backend->sleep) {
      backend->sleep(mlib_microseconds_count(duration));
   } else {
      mlib_sleep_for(duration);
   }
}


#endif /* MONGO_C_DRIVER_COMMON_THREAD_PRIVATE_H */
