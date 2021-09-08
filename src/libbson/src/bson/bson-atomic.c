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

#ifdef BSON_OS_UNIX
/* For sched_yield() */
#include <sched.h>
#endif

int32_t
bson_atomic_int_add (volatile int32_t *p, int32_t n)
{
   return n + bson_atomic_int32_fetch_add (p, n, bson_memory_order_seq_cst);
}

int64_t
bson_atomic_int64_add (volatile int64_t *p, int64_t n)
{
   return n + bson_atomic_int64_fetch_add (p, n, bson_memory_order_seq_cst);
}

void
bson_thrd_yield (void)
{
   BSON_IF_WINDOWS (SwitchToThread ();)
   BSON_IF_POSIX (sched_yield ();)
}

void
bson_memory_barrier (void)
{
   bson_atomic_thread_fence ();
}

/**
 * 32-bit x86 does not support 64-bit atomic integer operations.
 * We emulate that here using a spin lock and regular arithmetic operations
 */
static int g64bitAtomicLock = 0;

static void
_lock_64bit_atomic ()
{
   int i;
   if (bson_atomic_int_compare_exchange_weak (
          &g64bitAtomicLock, 0, 1, bson_memory_order_acquire) == 0) {
      /* Successfully took the spinlock */
      return;
   }
   /* Failed. Try taking ten more times, then begin sleeping. */
   for (i = 0; i < 10; ++i) {
      if (bson_atomic_int_compare_exchange_weak (
             &g64bitAtomicLock, 0, 1, bson_memory_order_acquire) == 0) {
         /* Succeeded in taking the lock */
         return;
      }
   }
   /* Still don't have the lock. Spin and yield */
   while (bson_atomic_int_compare_exchange_weak (
             &g64bitAtomicLock, 0, 1, bson_memory_order_acquire) != 0) {
      bson_thrd_yield ();
   }
}

static void
_unlock_64bit_atomic ()
{
   int64_t rv = bson_atomic_int_exchange (
      &g64bitAtomicLock, 0, bson_memory_order_release);
   BSON_ASSERT (rv == 1 && "Released atomic lock while not holding it");
}

int64_t
_bson_emul_atomic_int64_fetch_add (volatile int64_t *p,
                                   int64_t n,
                                   enum bson_memory_order _unused)
{
   int64_t ret;
   _lock_64bit_atomic ();
   ret = *p;
   *p += n;
   _unlock_64bit_atomic ();
   return ret;
}

int64_t
_bson_emul_atomic_int64_exchange (volatile int64_t *p,
                                  int64_t n,
                                  enum bson_memory_order _unused)
{
   int64_t ret;
   _lock_64bit_atomic ();
   ret = *p;
   *p = n;
   _unlock_64bit_atomic ();
   return ret;
}

int64_t
_bson_emul_atomic_int64_compare_exchange_strong (volatile int64_t *p,
                                                 int64_t expect_value,
                                                 int64_t new_value,
                                                 enum bson_memory_order _unused)
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

int64_t
_bson_emul_atomic_int64_compare_exchange_weak (volatile int64_t *p,
                                               int64_t expect_value,
                                               int64_t new_value,
                                               enum bson_memory_order order)
{
   /* We're emulating. We can't do a weak version. */
   return _bson_emul_atomic_int64_compare_exchange_strong (
      p, expect_value, new_value, order);
}
