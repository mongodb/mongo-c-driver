/* POSIX runtime adapter for the thread backend contract tests. */
#ifndef MCOMMON_TEST_THREAD_BACKEND_RUNTIME_H
#define MCOMMON_TEST_THREAD_BACKEND_RUNTIME_H

#include "./thread-backend-pthread.h"

#include <bson/bson-thread-backend.h>

#include <pthread.h>

#include <string.h>

/* Keep backend objects outside driver storage: even a pointer may not fit. */
typedef struct runtime_entry {
   void *key;
   struct runtime_entry *next;
   pthread_once_t once;
   mcommon_thread_backend_mutex_t mutex;
   mcommon_thread_backend_shared_mutex_t shared;
   mcommon_thread_backend_cond_t cond;
} runtime_entry;

static pthread_mutex_t runtime_registry_mutex = PTHREAD_MUTEX_INITIALIZER;
static runtime_entry *runtime_entries;
static bool runtime_once_used;
static bool runtime_mutex_init_used;

static runtime_entry *
runtime_lookup(void *key, bool create, bool is_once)
{
   backend_check(pthread_mutex_lock(&runtime_registry_mutex));
   runtime_entry *entry = runtime_entries;
   while (entry && entry->key != key) {
      entry = entry->next;
   }
   if (!entry && create) {
      const pthread_once_t initial = PTHREAD_ONCE_INIT;
      entry = backend_alloc(sizeof *entry);
      memset(entry, 0, sizeof *entry);
      entry->key = key;
      entry->once = initial;
      entry->next = runtime_entries;
      runtime_entries = entry;
   }
   if (!entry) {
      abort();
   }
   if (is_once) {
      runtime_once_used = true;
   }
   backend_check(pthread_mutex_unlock(&runtime_registry_mutex));
   return entry;
}

static void
runtime_remove(void *key)
{
   backend_check(pthread_mutex_lock(&runtime_registry_mutex));
   runtime_entry **entry = &runtime_entries;
   while (*entry && (*entry)->key != key) {
      entry = &(*entry)->next;
   }
   if (!*entry) {
      abort();
   }
   runtime_entry *removed = *entry;
   *entry = removed->next;
   free(removed);
   backend_check(pthread_mutex_unlock(&runtime_registry_mutex));
}

/* Release process-lifetime entries (including the logging mutex) at test exit,
 * after the last driver operation. */
static void
runtime_cleanup(void)
{
   while (runtime_entries) {
      runtime_entry *entry = runtime_entries;
      if (entry->mutex) {
         mcommon_thread_backend_mutex_destroy(&entry->mutex);
      }
      if (entry->shared) {
         mcommon_thread_backend_shared_mutex_destroy(&entry->shared);
      }
      if (entry->cond) {
         mcommon_thread_backend_cond_destroy(&entry->cond);
      }
      runtime_entries = entry->next;
      free(entry);
   }
}

static void
runtime_once(void *storage, void (*callback)(void))
{
   backend_check(pthread_once(&runtime_lookup(storage, true, true)->once, callback));
}

static int
runtime_thread_create(void *storage, bson_thread_backend_function_t callback, void *arg)
{
   pthread_t *thread = backend_alloc(sizeof *thread);
   int result = pthread_create(thread, NULL, callback, arg);
   if (result) {
      free(thread);
   } else {
      *(void **)storage = thread;
   }
   return result;
}

static int
runtime_thread_join(void *storage)
{
   pthread_t *thread = *(void **)storage;
   int result = pthread_join(*thread, NULL);
   if (result == 0) {
      free(thread);
   }
   return result;
}

static void
runtime_mutex_init(void *storage)
{
   backend_check(pthread_mutex_lock(&runtime_registry_mutex));
   runtime_mutex_init_used = true;
   backend_check(pthread_mutex_unlock(&runtime_registry_mutex));
   mcommon_thread_backend_mutex_init(&runtime_lookup(storage, true, false)->mutex);
}

static void
runtime_mutex_destroy(void *storage)
{
   mcommon_thread_backend_mutex_destroy(&runtime_lookup(storage, false, false)->mutex);
   runtime_remove(storage);
}

#define RUNTIME_ADAPT(name, field)                                                  \
   static void runtime_##name(void *storage)                                        \
   {                                                                                \
      mcommon_thread_backend_##name(&runtime_lookup(storage, false, false)->field); \
   }
RUNTIME_ADAPT(mutex_lock, mutex)
RUNTIME_ADAPT(mutex_unlock, mutex)
RUNTIME_ADAPT(shared_mutex_lock, shared)
RUNTIME_ADAPT(shared_mutex_unlock, shared)
RUNTIME_ADAPT(shared_mutex_lock_shared, shared)
RUNTIME_ADAPT(shared_mutex_unlock_shared, shared)
RUNTIME_ADAPT(cond_signal, cond)
RUNTIME_ADAPT(cond_broadcast, cond)
#undef RUNTIME_ADAPT

static bool
runtime_mutex_is_locked(void *storage)
{
   return mcommon_thread_backend_mutex_is_locked(&runtime_lookup(storage, false, false)->mutex);
}

static void
runtime_shared_mutex_init(void *storage)
{
   mcommon_thread_backend_shared_mutex_init(&runtime_lookup(storage, true, false)->shared);
}

static void
runtime_shared_mutex_destroy(void *storage)
{
   mcommon_thread_backend_shared_mutex_destroy(&runtime_lookup(storage, false, false)->shared);
   runtime_remove(storage);
}

static void
runtime_cond_init(void *storage)
{
   mcommon_thread_backend_cond_init(&runtime_lookup(storage, true, false)->cond);
}

static void
runtime_cond_destroy(void *storage)
{
   mcommon_thread_backend_cond_destroy(&runtime_lookup(storage, false, false)->cond);
   runtime_remove(storage);
}

static int
runtime_cond_wait(void *cond, void *mutex)
{
   return mcommon_thread_backend_cond_wait(&runtime_lookup(cond, false, false)->cond,
                                           &runtime_lookup(mutex, false, false)->mutex);
}

static int
runtime_cond_timedwait(void *cond, void *mutex, int64_t timeout_ms)
{
   return mcommon_thread_backend_cond_timedwait(
      &runtime_lookup(cond, false, false)->cond, &runtime_lookup(mutex, false, false)->mutex, timeout_ms);
}

static const bson_thread_backend_t runtime_thread_backend = {
   .struct_size = sizeof(bson_thread_backend_t),
   .once = runtime_once,
   .thread_create = runtime_thread_create,
   .thread_join = runtime_thread_join,
   .mutex_init = runtime_mutex_init,
   .mutex_destroy = runtime_mutex_destroy,
   .mutex_lock = runtime_mutex_lock,
   .mutex_unlock = runtime_mutex_unlock,
   .mutex_is_locked = runtime_mutex_is_locked,
   .shared_mutex_init = runtime_shared_mutex_init,
   .shared_mutex_destroy = runtime_shared_mutex_destroy,
   .shared_mutex_lock = runtime_shared_mutex_lock,
   .shared_mutex_unlock = runtime_shared_mutex_unlock,
   .shared_mutex_lock_shared = runtime_shared_mutex_lock_shared,
   .shared_mutex_unlock_shared = runtime_shared_mutex_unlock_shared,
   .cond_init = runtime_cond_init,
   .cond_destroy = runtime_cond_destroy,
   .cond_wait = runtime_cond_wait,
   .cond_timedwait = runtime_cond_timedwait,
   .cond_is_timedout = mcommon_thread_backend_cond_is_timedout,
   .cond_signal = runtime_cond_signal,
   .cond_broadcast = runtime_cond_broadcast,
   .sleep = mcommon_thread_backend_sleep,
};

#endif
