/**
 * @file mlib/intutil.h
 * @brief Integer utilities
 * @date 2025-01-28
 *
 * @copyright Copyright 2009-present MongoDB, Inc.
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
#ifndef MLIB_INTUTIL_H_INCLUDED
#define MLIB_INTUTIL_H_INCLUDED

#include <limits.h>
#include <stdint.h>
#include <stdbool.h>

#include <mlib/config.h>

/**
 * @brief Given an integral type, evaluates to `true` if that type is signed,
 * otherwise `false`
 */
#define mlib_is_signed(T) (!((T) (-1) > 0))

// clang-format off
/**
 * @brief Given an integral type, yield an integral constant value representing
 * the maximal value of that type.
 */
#define mlib_maxof(T) \
   ((T) (mlib_is_signed (T) \
        ? ((T) ((((T) 1 << (sizeof (T) * CHAR_BIT - 2)) - 1) * 2 + 1)) \
        : ((T) ~(T) 0)))

/**
 * @brief Given an integral type, yield an integral constant value for the
 * minimal value of that type.
 */
#define mlib_minof(T) \
   MLIB_PRAGMA_IF_MSVC (warning (push)) \
   MLIB_PRAGMA_IF_MSVC (warning (disable : 4146)) \
   ((T) (!mlib_is_signed (T) \
        ? (T) 0 \
        : (T) (-((((T) 1 << (sizeof (T) * CHAR_BIT - 2)) - 1) * 2 + 1) - 1))) \
   MLIB_PRAGMA_IF_MSVC (warning (pop))
// clang-format on

/**
 * @brief A container for an integer that has been "scaled up" to maximum precision
 *
 * Don't create this manually. Instead, use `mlib_upsize_integer` to do it automatically
 */
typedef struct mlib_upsized_integer {
   union {
      // The signed value of the integer
      intmax_t s;
      // The unsigned value of the integer
      uintmax_t u;
   } i;
   // Whether the upscaled integer is stored in the signed field or the unsigned field
   bool is_signed;
} mlib_upsized_integer;

// clang-format off
/**
 * @brief Create an "upsized" version of an integer, normalizing all integral
 * values into a single type so that we can deduplicate functions that operate
 * on disparate integer types.
 *
 * Details: The integer is upcast into the maximum precision integer type (intmax_t). If
 * the operand is smaller than `intmax_t`, we assume that casting to the signed `intmax_t`
 * is always safe, even if the operand is unsigned, since e.g. a u32 can always be cast to
 * an i64 losslessly.
 *
 * If the integer to upcast is the same size as `intmax_t`, we need to decide whether to store
 * it as unsigned. The expression `(_mlibGetOne(Value)) - 2 < 1` will be `true` iff the operand is signed,
 * otherwise false. If the operand is signed, we can safely cast to `intmax_t` (it probably already
 * is of that type), otherwise, we can to `uintmax_t` and the returned `mlib_upsized_integer` will
 * indicate that the stored value is unsigned. The expression `1 - 2 < 1` is chosen
 * to avoid `-Wtype-limits` warnings from some compilers about unsigned comparison.
 */
#define mlib_upsize_integer(Value) \
   MLIB_PRAGMA_IF_MSVC (warning(push)) \
   MLIB_PRAGMA_IF_MSVC (warning(disable : 4189)) \
   /* NOLINTNEXTLINE(bugprone-sizeof-expression) */ \
   ((sizeof ((Value)) < sizeof (intmax_t) || (_mlibGetOne(Value) - 2) < _mlibGetOne(Value)) \
      ? mlib_init(mlib_upsized_integer) {{(intmax_t) (Value)}, true} \
      : mlib_init(mlib_upsized_integer) {{(intmax_t) (uintmax_t) (Value)}}) \
   MLIB_PRAGMA_IF_MSVC (warning(pop))
// Yield a 1 value of similar-ish type to the given expression. The ternary
// forces an integer promotion of literal 1 match the type of `V`, while leaving
// `V` unevaluated. Note that this will also promote `V` to be at least `(unsigned) int`,
// so the 1 value is only "similar" to `V`, and may be of a larger type
#define _mlibGetOne(V) (1 ? 1 : (V))
// clang-format on

#endif // MLIB_INTUTIL_H_INCLUDED
