/*
 * Copyright 2014 MongoDB, Inc.
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


#include "bson-atomic.h"


/*
 * We should only ever hit these on non-Windows systems, for which we require
 * pthread support. Therefore, we will avoid making a threading portability
 * for threads here and just use pthreads directly.
 */


#ifdef __BSON_NEED_BARRIER
#include <pthread.h>
static pthread_mutex_t gBarrier = PTHREAD_MUTEX_INITIALIZER;
void
bson_memory_barrier (void)
{
   pthread_mutex_lock (&gBarrier);
   pthread_mutex_unlock (&gBarrier);
}
#endif


#ifdef __BSON_NEED_ATOMIC_32
#include <pthread.h>
static pthread_mutex_t gSync32 = PTHREAD_MUTEX_INITIALIZER;
int32_t
bson_atomic_int_add (volatile int32_t *p, int32_t n)
{
   int ret;

   pthread_mutex_lock (&gSync32);
   *p += n;
   ret = *p;
   pthread_mutex_unlock (&gSync32);

   return ret;
}
#endif


#ifdef __BSON_NEED_ATOMIC_64
#include <pthread.h>
static pthread_mutex_t gSync64 = PTHREAD_MUTEX_INITIALIZER;
int64_t
bson_atomic_int64_add (volatile int64_t *p, int64_t n)
{
   int64_t ret;

   pthread_mutex_lock (&gSync64);
   *p += n;
   ret = *p;
   pthread_mutex_unlock (&gSync64);

   return ret;
}
#endif


#if defined(_MSC_VER) && !defined(_M_X64)

/**
 * MSVC targeting 32-bit x86  does not support 64-bit atomic integer operations.
 * We emulate thath here using a spin lock and regular arithmetic operations
 */
static int g64bitAtomicLock = 0;

static _lock_64bit_atomic ()
{
   while (!bson_atomic_int_compare_exchange (
      &g64bitAtomicLock, 0, 1, bson_memorder_acquire)) {
      SwitchToThread ();
   }
}

static _unlock_64bit_atomic ()
{
   int64_t rv =
      bson_atomic_int_exchange (&g64bitAtomicLock, 0, bson_memorder_release);
   BSON_ASSERT (rv == 1 && "Released atomic lock while not holding it");
}

int64_t
bson_atomic_int64_fetch_add (volatile int64_t *p,
                             int64_t n,
                             enum bson_atomic_memorder _unused)
{
   int64_t ret;
   _lock_64bit_atomic ();
   ret = *p;
   *p += n;
   _unlock_64bit_atomic ();
   return ret;
}

int64_t
bson_atomic_int64_exchange (volatile int64_t *p,
                            int64_t n,
                            enum bson_atomic_memorder _unused)
{
   int64_t ret;
   _lock_64bit_atomic ();
   ret = *p;
   *p = n;
   _unlock_64bit_atomic ();
   return ret;
}

int64_t
bson_atomic_int64_compare_exchange (volatile int64_t *p,
                                    int64_t expect_value,
                                    int64_t new_value,
                                    enum bson_atomic_memorder _unused)
{
   int64_t ret;
   _lock_64bit_atomic ();
   ret = *p;
   if (ret == expect_value) {
      *p = new_value;
   }
   _unlock_64bit_atomic ();
   return ret;
}

#endif
