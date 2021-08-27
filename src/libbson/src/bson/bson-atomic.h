/*
 * Copyright 2013-2014 MongoDB, Inc.
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

#include "bson-prelude.h"


#ifndef BSON_ATOMIC_H
#define BSON_ATOMIC_H


#include "bson-config.h"
#include "bson-compat.h"
#include "bson-macros.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif


BSON_BEGIN_DECLS


enum bson_atomic_memorder {
   bson_memorder_seqcst,
   bson_memorder_acquire,
   bson_memorder_release,
   bson_memorder_relaxed
};

#if _M_ARM /* MSVC memorder atomics are only avail on ARM */
#define MSVC_MEMORDER_SUFFIX(X) X
#else
#define MSVC_MEMORDER_SUFFIX(X)
#endif


#define DEF_ATOMIC_OP(MSVC_Intrinsic, GNU_Intrinsic, Order, ...)             \
   do {                                                                      \
      switch (Order) {                                                       \
      case bson_memorder_seqcst:                                             \
         BSON_IF_MSVC (return MSVC_Intrinsic (__VA_ARGS__);)                 \
         BSON_IF_GNU (return GNU_Intrinsic (__VA_ARGS__, __ATOMIC_SEQ_CST);) \
      case bson_memorder_acquire:                                            \
         BSON_IF_MSVC (                                                      \
            return BSON_CONCAT (MSVC_Intrinsic,                              \
                                MSVC_MEMORDER_SUFFIX (_acq)) (__VA_ARGS__);) \
         BSON_IF_GNU (return GNU_Intrinsic (__VA_ARGS__, __ATOMIC_ACQUIRE);) \
      case bson_memorder_release:                                            \
         BSON_IF_MSVC (                                                      \
            return BSON_CONCAT (MSVC_Intrinsic,                              \
                                MSVC_MEMORDER_SUFFIX (_rel)) (__VA_ARGS__);) \
         BSON_IF_GNU (return GNU_Intrinsic (__VA_ARGS__, __ATOMIC_RELEASE);) \
      case bson_memorder_relaxed:                                            \
         BSON_IF_MSVC (                                                      \
            return BSON_CONCAT (MSVC_Intrinsic,                              \
                                MSVC_MEMORDER_SUFFIX (_nf)) (__VA_ARGS__);)  \
         BSON_IF_GNU (return GNU_Intrinsic (__VA_ARGS__, __ATOMIC_RELAXED);) \
      }                                                                      \
   } while (0)


#define DEF_ATOMIC_CMPEXCH(                                              \
   VCSuffix1, VCSuffix2, GNU_MemOrder, Ptr, ExpectActualVar, NewValue)   \
   BSON_IF_MSVC (ExpectActualVar = BSON_CONCAT3 (                        \
                    _InterlockedCompareExchange, VCSuffix1, VCSuffix2) ( \
                    Ptr, NewValue, ExpectActualVar);)                    \
   BSON_IF_GNU (__atomic_compare_exchange_n (                            \
      Ptr, &ExpectActualVar, NewValue, false, GNU_MemOrder, GNU_MemOrder));


#define DECL_ATOMIC_INTEGRAL(NamePart, Type, VCIntrinSuffix)                   \
   static BSON_INLINE Type bson_atomic_##NamePart##_fetch_add (                \
      volatile Type *a, Type addend, enum bson_atomic_memorder ord)            \
   {                                                                           \
      DEF_ATOMIC_OP (BSON_CONCAT (_InterlockedExchangeAdd, VCIntrinSuffix),    \
                     __atomic_fetch_add,                                       \
                     ord,                                                      \
                     a,                                                        \
                     addend);                                                  \
   }                                                                           \
                                                                               \
   static BSON_INLINE Type bson_atomic_##NamePart##_fetch_sub (                \
      volatile Type *a, Type subtrahend, enum bson_atomic_memorder ord)        \
   {                                                                           \
      /* MSVC doesn't have a subtract intrinsic, so just reuse addition    */  \
      BSON_IF_MSVC (                                                           \
         return bson_atomic_##NamePart##_fetch_add (a, -subtrahend, ord);)     \
      BSON_IF_GNU (DEF_ATOMIC_OP (~, __atomic_fetch_sub, ord, a, subtrahend);) \
   }                                                                           \
                                                                               \
   static BSON_INLINE Type bson_atomic_##NamePart##_fetch (                    \
      Type const volatile *a, enum bson_atomic_memorder order)                 \
   {                                                                           \
      /* MSVC doesn't have a load intrinsic, so just add zero */               \
      BSON_IF_MSVC (return bson_atomic_##NamePart##_fetch_add (                \
                              (Type volatile *) a, 0, order);)                 \
      /* GNU doesn't want RELEASE order for the fetch operation, so we can't   \
       * just use DEF_ATOMIC_OP. */                                            \
      BSON_IF_GNU (switch (order) {                                            \
         case bson_memorder_release: /* Fall back to seqcst */                 \
         case bson_memorder_seqcst:                                            \
            return __atomic_load_n (a, __ATOMIC_SEQ_CST);                      \
         case bson_memorder_acquire:                                           \
            return __atomic_load_n (a, __ATOMIC_ACQUIRE);                      \
         case bson_memorder_relaxed:                                           \
            return __atomic_load_n (a, __ATOMIC_RELAXED);                      \
      })                                                                       \
   }                                                                           \
                                                                               \
   static BSON_INLINE Type bson_atomic_##NamePart##_exchange (                 \
      volatile Type *a, Type value, enum bson_atomic_memorder ord)             \
   {                                                                           \
      DEF_ATOMIC_OP (BSON_CONCAT (_InterlockedExchange, VCIntrinSuffix),       \
                     __atomic_exchange_n,                                      \
                     ord,                                                      \
                     a,                                                        \
                     value);                                                   \
   }                                                                           \
                                                                               \
   static BSON_INLINE Type bson_atomic_##NamePart##_compare_exchange (         \
      volatile Type *a,                                                        \
      Type expect,                                                             \
      Type new_value,                                                          \
      enum bson_atomic_memorder ord)                                           \
   {                                                                           \
      Type actual = expect;                                                    \
      switch (ord) {                                                           \
      case bson_memorder_release:                                              \
      case bson_memorder_seqcst:                                               \
         DEF_ATOMIC_CMPEXCH (                                                  \
            VCIntrinSuffix, , __ATOMIC_SEQ_CST, a, actual, new_value);         \
         break;                                                                \
      case bson_memorder_acquire:                                              \
         DEF_ATOMIC_CMPEXCH (VCIntrinSuffix,                                   \
                             MSVC_MEMORDER_SUFFIX (_acq),                      \
                             __ATOMIC_ACQUIRE,                                 \
                             a,                                                \
                             actual,                                           \
                             new_value);                                       \
         break;                                                                \
      case bson_memorder_relaxed:                                              \
         DEF_ATOMIC_CMPEXCH (VCIntrinSuffix,                                   \
                             MSVC_MEMORDER_SUFFIX (_nf),                       \
                             __ATOMIC_RELAXED,                                 \
                             a,                                                \
                             actual,                                           \
                             new_value);                                       \
         break;                                                                \
      }                                                                        \
      return actual;                                                           \
   }

#define DECL_ATOMIC_STDINT(Name, VCSuffix) \
   DECL_ATOMIC_INTEGRAL (Name, Name##_t, VCSuffix)

DECL_ATOMIC_STDINT (int8, 8);
DECL_ATOMIC_STDINT (int16, 16);
DECL_ATOMIC_STDINT (int32, );
#if !defined(_MSC_VER) || defined(_M_X64)
/* (MSVC 64-bit intrinsics are only available in x64) */
DECL_ATOMIC_STDINT (int64, 64);
#endif

DECL_ATOMIC_INTEGRAL (int, int, );

#undef DECL_ATOMIC_STDINT
#undef DECL_ATOMIC_INTEGRAL
#undef DEF_ATOMIC_OP
#undef DEF_ATOMIC_CMPEXCH
#undef MSVC_MEMORDER_SUFFIX

#if defined(__sun) && defined(__SVR4)
/* Solaris */
#include <atomic.h>
#define bson_atomic_int_add(p, v) \
   atomic_add_32_nv ((volatile uint32_t *) p, (v))
#define bson_atomic_int64_add(p, v) \
   atomic_add_64_nv ((volatile uint64_t *) p, (v))
#elif defined(_WIN32)
/* MSVC/MinGW */
#define __BSON_NEED_ATOMIC_WINDOWS
#else
#ifdef BSON_HAVE_ATOMIC_32_ADD_AND_FETCH
#define bson_atomic_int_add(p, v) __sync_add_and_fetch ((p), (v))
#else
#define __BSON_NEED_ATOMIC_32
#endif
#ifdef BSON_HAVE_ATOMIC_64_ADD_AND_FETCH
#if BSON_GNUC_IS_VERSION(4, 1)
/*
 * GCC 4.1 on i386 can generate buggy 64-bit atomic increment.
 * So we will work around with a fallback.
 *
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=40693
 */
#define __BSON_NEED_ATOMIC_64
#else
#define bson_atomic_int64_add(p, v) \
   __sync_add_and_fetch ((volatile int64_t *) (p), (int64_t) (v))
#endif
#else
#define __BSON_NEED_ATOMIC_64
#endif
#endif

#ifdef __BSON_NEED_ATOMIC_32
BSON_EXPORT (int32_t)
bson_atomic_int_add (volatile int32_t *p, int32_t n);
#endif
#ifdef __BSON_NEED_ATOMIC_64
BSON_EXPORT (int64_t)
bson_atomic_int64_add (volatile int64_t *p, int64_t n);
#endif
/*
 * The logic above is such that __BSON_NEED_ATOMIC_WINDOWS should only be
 * defined if neither __BSON_NEED_ATOMIC_32 nor __BSON_NEED_ATOMIC_64 are.
 */
#ifdef __BSON_NEED_ATOMIC_WINDOWS
BSON_EXPORT (int32_t)
bson_atomic_int_add (volatile int32_t *p, int32_t n);
BSON_EXPORT (int64_t)
bson_atomic_int64_add (volatile int64_t *p, int64_t n);
#endif


#if defined(_WIN32)
#define bson_memory_barrier() MemoryBarrier ()
#elif defined(__GNUC__)
#if BSON_GNUC_CHECK_VERSION(4, 1)
#define bson_memory_barrier() __sync_synchronize ()
#else
#warning "GCC Pre-4.1 discovered, using inline assembly for memory barrier."
#define bson_memory_barrier() __asm__ volatile("" ::: "memory")
#endif
#elif defined(__SUNPRO_C)
#include <mbarrier.h>
#define bson_memory_barrier() __machine_rw_barrier ()
#elif defined(__xlC__)
#define bson_memory_barrier() __sync ()
#else
#define __BSON_NEED_BARRIER 1
#warning "Unknown compiler, using lock for compiler barrier."
BSON_EXPORT (void)
bson_memory_barrier (void);
#endif


BSON_END_DECLS


#endif /* BSON_ATOMIC_H */
