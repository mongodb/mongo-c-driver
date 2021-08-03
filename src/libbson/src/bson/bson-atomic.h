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
         BSON_IF_GNU_LIKE (                                                  \
            return GNU_Intrinsic (__VA_ARGS__, __ATOMIC_SEQ_CST);)           \
      case bson_memorder_acquire:                                            \
         BSON_IF_MSVC (                                                      \
            return BSON_CONCAT (MSVC_Intrinsic,                              \
                                MSVC_MEMORDER_SUFFIX (_acq)) (__VA_ARGS__);) \
         BSON_IF_GNU_LIKE (                                                  \
            return GNU_Intrinsic (__VA_ARGS__, __ATOMIC_ACQUIRE);)           \
      case bson_memorder_release:                                            \
         BSON_IF_MSVC (                                                      \
            return BSON_CONCAT (MSVC_Intrinsic,                              \
                                MSVC_MEMORDER_SUFFIX (_rel)) (__VA_ARGS__);) \
         BSON_IF_GNU_LIKE (                                                  \
            return GNU_Intrinsic (__VA_ARGS__, __ATOMIC_RELEASE);)           \
      case bson_memorder_relaxed:                                            \
         BSON_IF_MSVC (                                                      \
            return BSON_CONCAT (MSVC_Intrinsic,                              \
                                MSVC_MEMORDER_SUFFIX (_nf)) (__VA_ARGS__);)  \
         BSON_IF_GNU_LIKE (                                                  \
            return GNU_Intrinsic (__VA_ARGS__, __ATOMIC_RELAXED);)           \
      }                                                                      \
   } while (0)


#define DEF_ATOMIC_CMPEXCH(                                              \
   VCSuffix1, VCSuffix2, GNU_MemOrder, Ptr, ExpectActualVar, NewValue)   \
   BSON_IF_MSVC (ExpectActualVar = BSON_CONCAT3 (                        \
                    _InterlockedCompareExchange, VCSuffix1, VCSuffix2) ( \
                    Ptr, NewValue, ExpectActualVar);)                    \
   BSON_IF_GNU_LIKE (__atomic_compare_exchange_n (                       \
      Ptr, &ExpectActualVar, NewValue, false, GNU_MemOrder, GNU_MemOrder));


#define DECL_ATOMIC_INTEGRAL(NamePart, Type, VCIntrinSuffix)                  \
   static BSON_INLINE Type bson_atomic_##NamePart##_fetch_add (               \
      Type volatile *a, Type addend, enum bson_atomic_memorder ord)           \
   {                                                                          \
      DEF_ATOMIC_OP (BSON_CONCAT (_InterlockedExchangeAdd, VCIntrinSuffix),   \
                     __atomic_fetch_add,                                      \
                     ord,                                                     \
                     a,                                                       \
                     addend);                                                 \
   }                                                                          \
                                                                              \
   static BSON_INLINE Type bson_atomic_##NamePart##_fetch_sub (               \
      Type volatile *a, Type subtrahend, enum bson_atomic_memorder ord)       \
   {                                                                          \
      /* MSVC doesn't have a subtract intrinsic, so just reuse addition    */ \
      BSON_IF_MSVC (                                                          \
         return bson_atomic_##NamePart##_fetch_add (a, -subtrahend, ord);)    \
      BSON_IF_GNU_LIKE (                                                      \
         DEF_ATOMIC_OP (~, __atomic_fetch_sub, ord, a, subtrahend);)          \
   }                                                                          \
                                                                              \
   static BSON_INLINE Type bson_atomic_##NamePart##_fetch (                   \
      Type volatile *a, enum bson_atomic_memorder order)                      \
   {                                                                          \
      /* MSVC doesn't have a load intrinsic, so just add zero */              \
      BSON_IF_MSVC (return bson_atomic_##NamePart##_fetch_add (a, 0, order);) \
      /* GNU doesn't want RELEASE order for the fetch operation, so we can't  \
       * just use DEF_ATOMIC_OP. */                                           \
      BSON_IF_GNU_LIKE (switch (order) {                                      \
         case bson_memorder_release: /* Fall back to seqcst */                \
         case bson_memorder_seqcst:                                           \
            return __atomic_load_n (a, __ATOMIC_SEQ_CST);                     \
         case bson_memorder_acquire:                                          \
            return __atomic_load_n (a, __ATOMIC_ACQUIRE);                     \
         case bson_memorder_relaxed:                                          \
            return __atomic_load_n (a, __ATOMIC_RELAXED);                     \
      })                                                                      \
   }                                                                          \
                                                                              \
   static BSON_INLINE Type bson_atomic_##NamePart##_exchange (                \
      Type volatile *a, Type value, enum bson_atomic_memorder ord)            \
   {                                                                          \
      DEF_ATOMIC_OP (BSON_CONCAT (_InterlockedExchange, VCIntrinSuffix),      \
                     __atomic_exchange_n,                                     \
                     ord,                                                     \
                     a,                                                       \
                     value);                                                  \
   }                                                                          \
                                                                              \
   static BSON_INLINE Type bson_atomic_##NamePart##_compare_exchange (        \
      Type volatile *a,                                                       \
      Type expect,                                                            \
      Type new_value,                                                         \
      enum bson_atomic_memorder ord)                                          \
   {                                                                          \
      Type actual = expect;                                                   \
      switch (ord) {                                                          \
      case bson_memorder_release:                                             \
      case bson_memorder_seqcst:                                              \
         DEF_ATOMIC_CMPEXCH (                                                 \
            VCIntrinSuffix, , __ATOMIC_SEQ_CST, a, actual, new_value);        \
         break;                                                               \
      case bson_memorder_acquire:                                             \
         DEF_ATOMIC_CMPEXCH (VCIntrinSuffix,                                  \
                             MSVC_MEMORDER_SUFFIX (_acq),                     \
                             __ATOMIC_ACQUIRE,                                \
                             a,                                               \
                             actual,                                          \
                             new_value);                                      \
         break;                                                               \
      case bson_memorder_relaxed:                                             \
         DEF_ATOMIC_CMPEXCH (VCIntrinSuffix,                                  \
                             MSVC_MEMORDER_SUFFIX (_nf),                      \
                             __ATOMIC_RELAXED,                                \
                             a,                                               \
                             actual,                                          \
                             new_value);                                      \
         break;                                                               \
      }                                                                       \
      return actual;                                                          \
   }

#define DECL_ATOMIC_STDINT(Name, VCSuffix) \
   DECL_ATOMIC_INTEGRAL (Name, Name##_t, VCSuffix)

DECL_ATOMIC_STDINT (int8, 8);
DECL_ATOMIC_STDINT (int16, 16);
DECL_ATOMIC_STDINT (int32, );
#if !defined(_MSC_VER) || defined(_M_X64)
/* (MSVC 64-bit intrinsics are only available in x64) */
DECL_ATOMIC_STDINT (int64, 64);
#else
extern int64_t
bson_atomic_int64_fetch_add (int64_t volatile *val,
                             int64_t v,
                             enum bson_atomic_memorder);
extern int64_t
bson_atomic_int64_exchange (int64_t volatile *val,
                            int64_t v,
                            enum bson_atomic_memorder);
extern int64_t
bson_atomic_int64_compare_exchange (int64_t volatile *val,
                                    int64_t expect_value,
                                    int64_t new_value,
                                    enum bson_atomic_memorder);
#endif

DECL_ATOMIC_INTEGRAL (int, int, );

static BSON_INLINE void *
bson_atomic_ptr_exchange (void *volatile *ptr,
                          void *new_value,
                          enum bson_atomic_memorder ord)
{
   DEF_ATOMIC_OP (
      _InterlockedExchangePointer, __atomic_exchange_n, ord, ptr, new_value);
}

static BSON_INLINE void *
bson_atomic_ptr_compare_exchange (void *volatile *ptr,
                                  void *expect,
                                  void *new_value,
                                  enum bson_atomic_memorder ord)
{
   switch (ord) {
   case bson_memorder_seqcst:
      DEF_ATOMIC_CMPEXCH (Pointer, , __ATOMIC_SEQ_CST, ptr, expect, new_value);
      return expect;
   case bson_memorder_relaxed:
      DEF_ATOMIC_CMPEXCH (Pointer,
                          MSVC_MEMORDER_SUFFIX (_nf),
                          __ATOMIC_RELAXED,
                          ptr,
                          expect,
                          new_value);
      return expect;
   case bson_memorder_acquire:
      DEF_ATOMIC_CMPEXCH (Pointer,
                          MSVC_MEMORDER_SUFFIX (_acq),
                          __ATOMIC_ACQUIRE,
                          ptr,
                          expect,
                          new_value);
      return expect;
   case bson_memorder_release:
      DEF_ATOMIC_CMPEXCH (Pointer,
                          MSVC_MEMORDER_SUFFIX (_rel),
                          __ATOMIC_RELEASE,
                          ptr,
                          expect,
                          new_value);
      return expect;
   }
}

#undef DECL_ATOMIC_STDINT
#undef DECL_ATOMIC_INTEGRAL
#undef DEF_ATOMIC_OP
#undef DEF_ATOMIC_CMPEXCH
#undef MSVC_MEMORDER_SUFFIX

/**
 * @brief Generate a full-fence memory barrier at the call site.
 */
static BSON_INLINE void
bson_memory_barrier ()
{
   BSON_IF_MSVC (MemoryBarrier ();)
   BSON_IF_GNU_LIKE (__sync_synchronize ();)
}


BSON_END_DECLS


#endif /* BSON_ATOMIC_H */
