/**
 * @file mlib/pp/if-else.h
 * @brief A "ternary" macro
 * @date 2025-10-31
 *
 * This presents macros for doing conditional expansions:
 *
 * • MLIB_IF(<cond>)(<then>)
 *      → Expands to `<then>` if-and-only-if `<cond>` is truthy
 * • MLIB_UNLESS(<cond>)(<then>)
 *      → Expands to `<then>` if-and-only-if `<cond>` is falsey
 * • MLIB_IF_ELSE(<cond>)(<then>)(<else>)
 *      → If `<cond>` is truthy, expands to `<then>`, otherwise `<else>`
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

#ifndef MLIB_PP_IF_ELSE_H_INCLUDED
#define MLIB_PP_IF_ELSE_H_INCLUDED

#include "./basic.h"   // paste, nothing
#include "./boolean.h" // boolean, negate

/**
 * @brief A ternary conditional macro. Expects three argument lists in sequence:
 *
 *  MLIB_IF_ELSE (<condition>) (<true-content>) (<false-content>)
 *
 * If the `<condition>` is a truthy token (see: `MLIB_BOOLEAN`), expands to
 * `<true-content>`. If `<condition>` is a falsey token, expands to `<false-content>`.
 * If `<condition>` is any other token, then a compilation error will occur.
 * The unused argument list is discarded.
 */
#define MLIB_IF_ELSE(...) MLIB_PASTE(_mlibIfElse_, MLIB_BOOLEAN(__VA_ARGS__))
// Expanded if the MLIB_BOOLEAN expands to `1`:
#define _mlibIfElse_1(...)                           \
   __VA_ARGS__     /* ← Expands the first arglist */ \
      MLIB_NOTHING /* ← Drops the next arglist */
// Expanded if the MLIB_BOOLEAN expands to `0`:
#define _mlibIfElse_0(...)       \
   /* Drops the first arglist */ \
   _mlibIfElseExpand /* ← Expand the next arglist */
/*-
 * Textually identical to `MLIB_JUST`, but deliberately a distinct macro: do not
 * "simplify" this away. `MLIB_IF_ELSE` is used inside recursive constructs (see
 * `mlib/pp/map.h`) whose expansion may already have a `MLIB_JUST` in flight. A
 * macro currently being expanded is "painted blue" and will not be expanded
 * again during its own rescan, so reusing `MLIB_JUST` here would silently leave
 * the else-branch unexpanded in exactly those cases. The separate name keeps
 * this expansion pass independent of any other.
 */
#define _mlibIfElseExpand(...) __VA_ARGS__

/**
 * @brief Expand content if-and-only-if the condition is truthy:
 *
 *    MLIB_IF (<condition>) (<content>)
 *
 * Where `<condition>` is evaluated using `MLIB_BOOLEAN`
 *
 * The trailing `()` reads backwards, so beware: it is the *then*-branch, and it
 * is empty. The user's `(<content>)` becomes the *else*-branch. Negating the
 * condition flips the sense back, so the net effect is "expand <content> when
 * <condition> is truthy", as documented.
 */
#define MLIB_IF(...) MLIB_IF_ELSE(MLIB_NEGATE(__VA_ARGS__))()
/**
 * @brief Expand content if-and-only-if the condition is falsey:
 *
 *    MLIB_UNLESS (<condition>) (<content>)
 *
 * Where `<condition>` is evaluated using `MLIB_BOOLEAN`
 *
 * The same trick as `MLIB_IF`, minus the negation: the empty `()` is the
 * then-branch that is taken (and discarded) when the condition is truthy, and
 * the user's `(<content>)` lands in the else-branch.
 */
#define MLIB_UNLESS(...) MLIB_IF_ELSE(__VA_ARGS__)()

#endif // MLIB_PP_IF_ELSE_H_INCLUDED
