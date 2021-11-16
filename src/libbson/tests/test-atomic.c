/*
 * Copyright 2013 MongoDB, Inc.
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


#include <bson/bson.h>

#include "TestSuite.h"

#define ATOMIC(Kind, Operation) BSON_CONCAT4 (bson_atomic_, Kind, _, Operation)


#define TEST_KIND_WITH_MEMORDER(Kind, TypeName, MemOrder, Assert)              \
   do {                                                                        \
      int i;                                                                   \
      TypeName got;                                                            \
      TypeName value = 0;                                                      \
      got = ATOMIC (Kind, fetch) (&value, MemOrder);                           \
      Assert (got, ==, 0);                                                     \
      got = ATOMIC (Kind, fetch_add) (&value, 42, MemOrder);                   \
      Assert (got, ==, 0);                                                     \
      Assert (value, ==, 42);                                                  \
      got = ATOMIC (Kind, fetch_sub) (&value, 7, MemOrder);                    \
      Assert (got, ==, 42);                                                    \
      Assert (value, ==, 35);                                                  \
      got = ATOMIC (Kind, exchange) (&value, 77, MemOrder);                    \
      Assert (got, ==, 35);                                                    \
      Assert (value, ==, 77);                                                  \
      /* Compare-exchange fail: */                                             \
      got = ATOMIC (Kind, compare_exchange_strong) (&value, 4, 9, MemOrder);   \
      Assert (got, ==, 77);                                                    \
      Assert (value, ==, 77);                                                  \
      /* Compare-exchange succeed: */                                          \
      got = ATOMIC (Kind, compare_exchange_strong) (&value, 77, 9, MemOrder);  \
      Assert (got, ==, 77);                                                    \
      Assert (value, ==, 9);                                                   \
      /* Compare-exchange fail: */                                             \
      got = ATOMIC (Kind, compare_exchange_weak) (&value, 8, 12, MemOrder);    \
      Assert (got, ==, 9);                                                     \
      Assert (value, ==, 9);                                                   \
      /* Compare-exchange-weak succeed: */                                     \
      /* 'weak' may fail spuriously, so it must *eventually* succeed */        \
      for (i = 0; i < 10000 && value != 53; ++i) {                             \
         got = ATOMIC (Kind, compare_exchange_weak) (&value, 9, 53, MemOrder); \
         Assert (got, ==, 9);                                                  \
      }                                                                        \
      /* Check that it evenutally succeeded */                                 \
      Assert (value, ==, 53);                                                  \
   } while (0)

#define TEST_INTEGER_KIND(Kind, TypeName, Assert)            \
   do {                                                      \
      MONGOC_ERROR ("  memory order: relaxed ... begin");    \
      TEST_KIND_WITH_MEMORDER (                              \
         Kind, TypeName, bson_memory_order_relaxed, Assert); \
      MONGOC_ERROR ("  memory order: relaxed ... end");      \
      MONGOC_ERROR ("  memory order: acq_rel ... begin");    \
      TEST_KIND_WITH_MEMORDER (                              \
         Kind, TypeName, bson_memory_order_acq_rel, Assert); \
      MONGOC_ERROR ("  memory order: acq_rel ... end");      \
      MONGOC_ERROR ("  memory order: acquire ... begin");    \
      TEST_KIND_WITH_MEMORDER (                              \
         Kind, TypeName, bson_memory_order_acquire, Assert); \
      MONGOC_ERROR ("  memory order: acquire ... end");      \
      MONGOC_ERROR ("  memory order: release ... begin");    \
      TEST_KIND_WITH_MEMORDER (                              \
         Kind, TypeName, bson_memory_order_release, Assert); \
      MONGOC_ERROR ("  memory order: release ... end");      \
      MONGOC_ERROR ("  memory order: consume ... begin");    \
      TEST_KIND_WITH_MEMORDER (                              \
         Kind, TypeName, bson_memory_order_consume, Assert); \
      MONGOC_ERROR ("  memory order: consume ... end");      \
      MONGOC_ERROR ("  memory order: seq_cst ... begin");    \
      TEST_KIND_WITH_MEMORDER (                              \
         Kind, TypeName, bson_memory_order_seq_cst, Assert); \
      MONGOC_ERROR ("  memory order: seq_cst ... end");      \
   } while (0)


static void
test_integers (void)
{
   MONGOC_ERROR ("test_integers: int64 ... begin");
   TEST_INTEGER_KIND (int64, int64_t, ASSERT_CMPINT64);
   MONGOC_ERROR ("test_integers: int64 ... end");
   MONGOC_ERROR ("test_integers: int32 ... begin");
   TEST_INTEGER_KIND (int32, int32_t, ASSERT_CMPINT32);
   MONGOC_ERROR ("test_integers: int32 ... end");
   MONGOC_ERROR ("test_integers: int16 ... begin");
   TEST_INTEGER_KIND (int16, int16_t, ASSERT_CMPINT);
   MONGOC_ERROR ("test_integers: int16 ... end");
   MONGOC_ERROR ("test_integers: int8 ... begin");
   TEST_INTEGER_KIND (int8, int8_t, ASSERT_CMPINT);
   MONGOC_ERROR ("test_integers: int8 ... end");
   MONGOC_ERROR ("test_integers: int ... begin");
   TEST_INTEGER_KIND (int, int, ASSERT_CMPINT);
   MONGOC_ERROR ("test_integers: int ... end");
}


static void
test_pointers (void)
{
   int u = 12;
   int v = 9;
   int w = 91;
   int *ptr = &v;
   int *other;
   int *prev;
   other = bson_atomic_ptr_fetch ((void *) &ptr, bson_memory_order_relaxed);
   ASSERT_CMPVOID (other, ==, ptr);
   prev =
      bson_atomic_ptr_exchange ((void *) &other, &u, bson_memory_order_relaxed);
   ASSERT_CMPVOID (prev, ==, &v);
   ASSERT_CMPVOID (other, ==, &u);
   prev = bson_atomic_ptr_compare_exchange_strong (
      (void *) &other, &v, &w, bson_memory_order_relaxed);
   ASSERT_CMPVOID (prev, ==, &u);
   ASSERT_CMPVOID (other, ==, &u);
   prev = bson_atomic_ptr_compare_exchange_strong (
      (void *) &other, &u, &w, bson_memory_order_relaxed);
   ASSERT_CMPVOID (prev, ==, &u);
   ASSERT_CMPVOID (other, ==, &w);
}


static void
test_thread_fence (void)
{
   bson_atomic_thread_fence ();
}

static void
test_thrd_yield (void)
{
   bson_thrd_yield ();
}


/**
 * Emulate atomic operations with a spin lock and regular arithmetic operations on systems that do not support compiler intrinsics.
 */
static int8_t gEmulAtomicLock = 0;

static void
_lock_emul_atomic ()
{
   int i;
   if (bson_atomic_int8_compare_exchange_weak (
          &gEmulAtomicLock, 0, 1, bson_memory_order_acquire) == 0) {
      /* Successfully took the spinlock */
      return;
   }
   /* Failed. Try taking ten more times, then begin sleeping. */
   for (i = 0; i < 10; ++i) {
      if (bson_atomic_int8_compare_exchange_weak (
             &gEmulAtomicLock, 0, 1, bson_memory_order_acquire) == 0) {
         /* Succeeded in taking the lock */
         return;
      }
   }
   /* Still don't have the lock. Spin and yield */
   while (bson_atomic_int8_compare_exchange_weak (
             &gEmulAtomicLock, 0, 1, bson_memory_order_acquire) != 0) {
      bson_thrd_yield ();
   }
}

static void
_unlock_emul_atomic ()
{
   int64_t rv = bson_atomic_int8_exchange (
      &gEmulAtomicLock, 0, bson_memory_order_release);
   BSON_ASSERT (rv == 1 && "Released atomic lock while not holding it");
}

static int32_t
bson_atomic_int32emul_fetch_add (volatile int32_t *p,
                                   int32_t n,
                                   enum bson_memory_order _unused)
{
   int32_t ret;
   _lock_emul_atomic ();
   ret = *p;
   *p += n;
   _unlock_emul_atomic ();
   return ret;
}

static int32_t
bson_atomic_int32emul_exchange (volatile int32_t *p,
                                  int32_t n,
                                  enum bson_memory_order _unused)
{
   int32_t ret;
   _lock_emul_atomic ();
   ret = *p;
   *p = n;
   _unlock_emul_atomic ();
   return ret;
}

static int32_t
bson_atomic_int32emul_compare_exchange_strong (volatile int32_t *p,
                                                 int32_t expect_value,
                                                 int32_t new_value,
                                                 enum bson_memory_order _unused)
{
   int32_t ret;
   _lock_emul_atomic ();
   ret = *p;
   if (ret == expect_value) {
      *p = new_value;
   }
   _unlock_emul_atomic ();
   return ret;
}

static int32_t
bson_atomic_int32emul_compare_exchange_weak (volatile int32_t *p,
                                               int32_t expect_value,
                                               int32_t new_value,
                                               enum bson_memory_order order)
{
   /* We're emulating. We can't do a weak version. */
   return bson_atomic_int32emul_compare_exchange_strong (
      p, expect_value, new_value, order);
}

static int32_t
bson_atomic_int32emul_fetch (const int32_t volatile *val,
                         enum bson_memory_order order)
{
   return bson_atomic_int32emul_fetch_add (
      (int32_t volatile *) val, 0, order);
}

static int32_t
bson_atomic_int32emul_fetch_sub (int32_t volatile *val,
                             int32_t v,
                             enum bson_memory_order order)
{
   return bson_atomic_int32emul_fetch_add (val, -v, order);
}

static void
test_integers_int32emul (void) {
   TEST_INTEGER_KIND (int32emul, int32_t, ASSERT_CMPINT32);
}

static int16_t
bson_atomic_int16emul_fetch_add (volatile int16_t *p,
                                 int16_t n,
                                 enum bson_memory_order _unused)
{
   int16_t ret;
   _lock_emul_atomic ();
   ret = *p;
   *p += n;
   _unlock_emul_atomic ();
   return ret;
}

static int16_t
bson_atomic_int16emul_exchange (volatile int16_t *p,
                                int16_t n,
                                enum bson_memory_order _unused)
{
   int16_t ret;
   _lock_emul_atomic ();
   ret = *p;
   *p = n;
   _unlock_emul_atomic ();
   return ret;
}

static int16_t
bson_atomic_int16emul_compare_exchange_strong (volatile int16_t *p,
                                               int16_t expect_value,
                                               int16_t new_value,
                                               enum bson_memory_order _unused)
{
   int16_t ret;
   _lock_emul_atomic ();
   ret = *p;
   if (ret == expect_value) {
      *p = new_value;
   }
   _unlock_emul_atomic ();
   return ret;
}

static int16_t
bson_atomic_int16emul_compare_exchange_weak (volatile int16_t *p,
                                             int16_t expect_value,
                                             int16_t new_value,
                                             enum bson_memory_order order)
{
   /* We're emulating. We can't do a weak version. */
   return bson_atomic_int16emul_compare_exchange_strong (
      p, expect_value, new_value, order);
}

static int16_t
bson_atomic_int16emul_fetch (const int16_t volatile *val,
                             enum bson_memory_order order)
{
   return bson_atomic_int16emul_fetch_add ((int16_t volatile *) val, 0, order);
}

static int16_t
bson_atomic_int16emul_fetch_sub (int16_t volatile *val,
                                 int16_t v,
                                 enum bson_memory_order order)
{
   return bson_atomic_int16emul_fetch_add (val, -v, order);
}

static void
test_integers_int16emul (void)
{
   TEST_INTEGER_KIND (int16emul, int16_t, ASSERT_CMPINT);
}

static int8_t
bson_atomic_int8emul_fetch_add (volatile int8_t *p,
                                int8_t n,
                                enum bson_memory_order _unused)
{
   int8_t ret;
   _lock_emul_atomic ();
   ret = *p;
   *p += n;
   _unlock_emul_atomic ();
   return ret;
}

static int8_t
bson_atomic_int8emul_exchange (volatile int8_t *p,
                               int8_t n,
                               enum bson_memory_order _unused)
{
   int8_t ret;
   _lock_emul_atomic ();
   ret = *p;
   *p = n;
   _unlock_emul_atomic ();
   return ret;
}

static int8_t
bson_atomic_int8emul_compare_exchange_strong (volatile int8_t *p,
                                              int8_t expect_value,
                                              int8_t new_value,
                                              enum bson_memory_order _unused)
{
   int8_t ret;
   _lock_emul_atomic ();
   ret = *p;
   if (ret == expect_value) {
      *p = new_value;
   }
   _unlock_emul_atomic ();
   return ret;
}

static int8_t
bson_atomic_int8emul_compare_exchange_weak (volatile int8_t *p,
                                            int8_t expect_value,
                                            int8_t new_value,
                                            enum bson_memory_order order)
{
   /* We're emulating. We can't do a weak version. */
   return bson_atomic_int8emul_compare_exchange_strong (
      p, expect_value, new_value, order);
}

static int8_t
bson_atomic_int8emul_fetch (const int8_t volatile *val,
                            enum bson_memory_order order)
{
   return bson_atomic_int8emul_fetch_add ((int8_t volatile *) val, 0, order);
}

static int8_t
bson_atomic_int8emul_fetch_sub (int8_t volatile *val,
                                int8_t v,
                                enum bson_memory_order order)
{
   return bson_atomic_int8emul_fetch_add (val, -v, order);
}

static void
test_integers_int8emul (void)
{
   TEST_INTEGER_KIND (int8emul, int8_t, ASSERT_CMPINT);
}

static int
bson_atomic_intemul_fetch_add (volatile int *p,
                               int n,
                               enum bson_memory_order _unused)
{
   int ret;
   _lock_emul_atomic ();
   ret = *p;
   *p += n;
   _unlock_emul_atomic ();
   return ret;
}

static int
bson_atomic_intemul_exchange (volatile int *p,
                              int n,
                              enum bson_memory_order _unused)
{
   int ret;
   _lock_emul_atomic ();
   ret = *p;
   *p = n;
   _unlock_emul_atomic ();
   return ret;
}

static int
bson_atomic_intemul_compare_exchange_strong (volatile int *p,
                                             int expect_value,
                                             int new_value,
                                             enum bson_memory_order _unused)
{
   int ret;
   _lock_emul_atomic ();
   ret = *p;
   if (ret == expect_value) {
      *p = new_value;
   }
   _unlock_emul_atomic ();
   return ret;
}

static int
bson_atomic_intemul_compare_exchange_weak (volatile int *p,
                                           int expect_value,
                                           int new_value,
                                           enum bson_memory_order order)
{
   /* We're emulating. We can't do a weak version. */
   return bson_atomic_intemul_compare_exchange_strong (
      p, expect_value, new_value, order);
}

static int
bson_atomic_intemul_fetch (const int volatile *val,
                           enum bson_memory_order order)
{
   return bson_atomic_intemul_fetch_add ((int volatile *) val, 0, order);
}

static int
bson_atomic_intemul_fetch_sub (int volatile *val,
                               int v,
                               enum bson_memory_order order)
{
   return bson_atomic_intemul_fetch_add (val, -v, order);
}

static void
test_integers_intemul (void)
{
   TEST_INTEGER_KIND (intemul, int, ASSERT_CMPINT);
}

static void
test_integers_int64 (void)
{
   TEST_INTEGER_KIND (int64, int64_t, ASSERT_CMPINT64);
}

static void
test_integers_int32 (void)
{
   TEST_INTEGER_KIND (int32, int32_t, ASSERT_CMPINT32);
}

static void
test_integers_int16 (void)
{
   TEST_INTEGER_KIND (int16, int16_t, ASSERT_CMPINT);
}

static void
test_integers_int8 (void)
{
   TEST_INTEGER_KIND (int8, int8_t, ASSERT_CMPINT);
}

static void
test_integers_int (void)
{
   TEST_INTEGER_KIND (int, int, ASSERT_CMPINT);
}

#define TEST_KIND_WITH_MEMORDER_FETCH(Kind, TypeName, MemOrder, Assert) \
   do {                                                                 \
      TypeName got;                                                     \
      TypeName value = 0;                                               \
      got = ATOMIC (Kind, fetch) (&value, MemOrder);                    \
      Assert (got, ==, 0);                                              \
   } while (0)


#define TEST_KIND_WITH_MEMORDER_FETCH_ADD(Kind, TypeName, MemOrder, Assert) \
   do {                                                                     \
      TypeName got;                                                         \
      TypeName value = 0;                                                   \
      got = ATOMIC (Kind, fetch_add) (&value, 42, MemOrder);                \
      Assert (got, ==, 0);                                                  \
      Assert (value, ==, 42);                                               \
   } while (0)


#define TEST_KIND_WITH_MEMORDER_FETCH_SUB(Kind, TypeName, MemOrder, Assert) \
   do {                                                                     \
      TypeName got;                                                         \
      TypeName value = 42;                                                  \
      got = ATOMIC (Kind, fetch_sub) (&value, 7, MemOrder);                 \
      Assert (got, ==, 42);                                                 \
      Assert (value, ==, 35);                                               \
   } while (0)


#define TEST_KIND_WITH_MEMORDER_EXCHANGE(Kind, TypeName, MemOrder, Assert) \
   do {                                                                    \
      TypeName got;                                                        \
      TypeName value = 35;                                                 \
      got = ATOMIC (Kind, exchange) (&value, 77, MemOrder);                \
      Assert (got, ==, 35);                                                \
      Assert (value, ==, 77);                                              \
   } while (0)


#define TEST_KIND_WITH_MEMORDER_COMPARE_EXCHANGE_STRONG(                      \
   Kind, TypeName, MemOrder, Assert)                                          \
   do {                                                                       \
      TypeName got;                                                           \
      TypeName value = 77;                                                    \
      /* Compare-exchange fail: */                                            \
      got = ATOMIC (Kind, compare_exchange_strong) (&value, 4, 9, MemOrder);  \
      Assert (got, ==, 77);                                                   \
      Assert (value, ==, 77);                                                 \
      /* Compare-exchange succeed: */                                         \
      got = ATOMIC (Kind, compare_exchange_strong) (&value, 77, 9, MemOrder); \
      Assert (got, ==, 77);                                                   \
      Assert (value, ==, 9);                                                  \
   } while (0)


#define TEST_KIND_WITH_MEMORDER_COMPARE_EXCHANGE_WEAK(                         \
   Kind, TypeName, MemOrder, Assert)                                           \
   do {                                                                        \
      int i;                                                                   \
      TypeName got;                                                            \
      TypeName value = 9;                                                      \
      /* Compare-exchange-weak succeed: */                                     \
      /* 'weak' may fail spuriously, so it must *eventually* succeed */        \
      for (i = 0; i < 10000 && value != 53; ++i) {                             \
         got = ATOMIC (Kind, compare_exchange_weak) (&value, 9, 53, MemOrder); \
         Assert (got, ==, 9);                                                  \
      }                                                                        \
      /* Check that it evenutally succeeded */                                 \
      Assert (value, ==, 53);                                                  \
   } while (0)

static void test_integers_int32_fetch (void) {
   TEST_KIND_WITH_MEMORDER_FETCH (int32, int32_t, bson_memory_order_relaxed, ASSERT_CMPINT32);
}
static void test_integers_int32_fetch_add (void) {
   TEST_KIND_WITH_MEMORDER_FETCH_ADD (int32, int32_t, bson_memory_order_relaxed, ASSERT_CMPINT32);
}
static void test_integers_int32_fetch_sub (void) {
   TEST_KIND_WITH_MEMORDER_FETCH_SUB (int32, int32_t, bson_memory_order_relaxed, ASSERT_CMPINT32);
}
static void test_integers_int32_exchange (void) {
   TEST_KIND_WITH_MEMORDER_EXCHANGE (int32, int32_t, bson_memory_order_relaxed, ASSERT_CMPINT32);
}
static void test_integers_int32_compare_exchange_strong (void) {
   TEST_KIND_WITH_MEMORDER_COMPARE_EXCHANGE_STRONG (int32, int32_t, bson_memory_order_relaxed, ASSERT_CMPINT32);
}
static void test_integers_int32_compare_exchange_weak (void) {
   TEST_KIND_WITH_MEMORDER_COMPARE_EXCHANGE_WEAK (int32, int32_t, bson_memory_order_relaxed, ASSERT_CMPINT32);
}

#if defined(__s390__)
#pragma message("DEFCHECK: __s390__ defined")
#endif

#if defined(__s390x__)
#pragma message("DEFCHECK: __s390x__ defined")
#endif

#if defined(__zarch__)
#pragma message("DEFCHECK: __zarch__ defined")
#endif

void
test_atomic_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/atomic/integers", test_integers);
   TestSuite_Add (suite, "/atomic/integers/int64", test_integers_int64);
   TestSuite_Add (suite, "/atomic/integers/int32", test_integers_int32);
   TestSuite_Add (suite, "/atomic/integers/int16", test_integers_int16);
   TestSuite_Add (suite, "/atomic/integers/int8", test_integers_int8);
   TestSuite_Add (suite, "/atomic/integers/int", test_integers_int);
   TestSuite_Add (suite, "/atomic/pointers", test_pointers);
   TestSuite_Add (suite, "/atomic/thread_fence", test_thread_fence);
   TestSuite_Add (suite, "/atomic/thread_yield", test_thrd_yield);
   TestSuite_Add (suite, "/atomic/integers/int32emul", test_integers_int32emul);
   TestSuite_Add (suite, "/atomic/integers/int16emul", test_integers_int16emul);
   TestSuite_Add (suite, "/atomic/integers/int8emul", test_integers_int8emul);
   TestSuite_Add (suite, "/atomic/integers/intemul", test_integers_intemul);
   TestSuite_Add (suite, "/atomic/integers/int32/fetch", test_integers_int32_fetch);
   TestSuite_Add (suite, "/atomic/integers/int32/fetch_add", test_integers_int32_fetch_add);
   TestSuite_Add (suite, "/atomic/integers/int32/fetch_sub", test_integers_int32_fetch_sub);
   TestSuite_Add (suite, "/atomic/integers/int32/exchange", test_integers_int32_exchange);
   TestSuite_Add (suite, "/atomic/integers/int32/compare_exchange_strong", test_integers_int32_compare_exchange_strong);
   TestSuite_Add (suite, "/atomic/integers/int32/compare_exchange_weak", test_integers_int32_compare_exchange_weak);
}
