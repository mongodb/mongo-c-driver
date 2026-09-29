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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static bson_thread_backend_t thread_backend;
static bool thread_backend_enabled;

const bson_thread_backend_t *
mcommon_thread_backend_get(void)
{
   return thread_backend_enabled ? &thread_backend : NULL;
}

bool
mcommon_thread_backend_set(const bson_thread_backend_t *backend)
{
   if (backend) {
      size_t size = backend->struct_size;
      if (size < sizeof backend->struct_size) {
         return false;
      }
      if (size > sizeof thread_backend) {
         size = sizeof thread_backend;
      }
      memset(&thread_backend, 0, sizeof thread_backend);
      memcpy(&thread_backend, backend, size);
      thread_backend_enabled = true;
   } else {
      thread_backend_enabled = false;
      memset(&thread_backend, 0, sizeof thread_backend);
   }
   return true;
}

#ifdef BSON_COMPILATION
void
bson_set_thread_backend(const bson_thread_backend_t *backend)
{
   mcommon_thread_backend_set(backend);
}
#endif

#if defined(BSON_OS_UNIX)
int
mcommon_thread_create(bson_thread_t *thread, BSON_THREAD_FUN_TYPE(func), void *arg)
{
   BSON_ASSERT_PARAM(thread);
   BSON_ASSERT_PARAM(func);
   BSON_OPTIONAL_PARAM(arg); // optional.
   const bson_thread_backend_t *backend = mcommon_thread_backend_get();
   if (backend && backend->thread_create) {
      return backend->thread_create(thread, func, arg);
   }
   pthread_t *native = malloc(sizeof *native);
   if (!native) {
      return ENOMEM;
   }
   int result = pthread_create(native, NULL, func, arg);
   if (result) {
      free(native);
   } else {
      *thread = native;
   }
   return result;
}
int
mcommon_thread_join(bson_thread_t thread)
{
   const bson_thread_backend_t *backend = mcommon_thread_backend_get();
   if (backend && backend->thread_join) {
      return backend->thread_join(&thread);
   }
   int result = pthread_join(*(pthread_t *)thread, NULL);
   if (result == 0) {
      free(thread);
   }
   return result;
}

#if defined(MONGOC_ENABLE_DEBUG_ASSERTIONS) && defined(BSON_OS_UNIX)
bool
mcommon_mutex_is_locked(bson_mutex_t *mutex)
{
   const bson_thread_backend_t *backend = mcommon_thread_backend_get();
   if (backend && backend->mutex_is_locked) {
      return backend->mutex_is_locked(mutex);
   }
   return mutex->valid_tid && pthread_equal(pthread_self(), mutex->lock_owner);
}
#endif

#else
int
mcommon_thread_create(bson_thread_t *thread, BSON_THREAD_FUN_TYPE(func), void *arg)
{
   const bson_thread_backend_t *backend = mcommon_thread_backend_get();
   if (backend && backend->thread_create) {
      return backend->thread_create(thread, func, arg);
   }
   BSON_ASSERT_PARAM(thread);
   BSON_ASSERT_PARAM(func);
   BSON_OPTIONAL_PARAM(arg); // optional.

   *thread = (HANDLE)_beginthreadex(NULL, 0, func, arg, 0, NULL);
   if (0 == *thread) {
      return errno;
   }
   return 0;
}
int
mcommon_thread_join(bson_thread_t thread)
{
   const bson_thread_backend_t *backend = mcommon_thread_backend_get();
   if (backend && backend->thread_join) {
      return backend->thread_join(&thread);
   }
   int ret;

   /* zero indicates success for WaitForSingleObject. */
   ret = WaitForSingleObject(thread, INFINITE);
   if (WAIT_OBJECT_0 != ret) {
      return ret;
   }
   /* zero indicates failure for CloseHandle. */
   ret = CloseHandle(thread);
   if (0 == ret) {
      return 1;
   }
   return 0;
}
#endif
