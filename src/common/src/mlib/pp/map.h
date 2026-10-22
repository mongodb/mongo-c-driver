/**
 * @file mlib/pp/map.h
 * @brief A MAP macro
 * @date 2025-10-31
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

#ifndef MLIB_PP_MAP_H_INCLUDED
#define MLIB_PP_MAP_H_INCLUDED

#include "./basic.h"    // nothing, eval
#include "./if-else.h"  // if_else
#include "./is-empty.h" // is_empty, has_comma

/**
 * @brief Perform a "macro map" expansion.
 *
 * Signature:
 *
 *    MLIB_MAP_MACRO(F, K, ...xs)
 *
 * **For each** Nth argument in `xs`, expands to approximately the following:
 *
 *    F(xs[N], K, N)
 *
 * That is, it "applies" the function (or function-like macro) `F` to each
 * argument in `xs`. The list element is passed as the first argument, the
 * argument `K` (the "constant") is always passed as the second argument, and the
 * third argument `N` is the zero-based index of the expansion (the "counter").
 *
 * If the argument `K` is not needed, it is common to pass a bogus token as a
 * placeholder, such as `~` or `@`, and simply ignore it in your definition of
 * `F`.
 *
 * @note The counter argument is not passed as a literal integer, but as a simple
 * series of `+1` that lengthens for each list item, i.e. `(0 + 1 + ... + 1)`.
 *
 * It is most common that `F` is itself another macro name that juggles the
 * argument list into some other form.
 *
 * @note No separator is inserted between the results. The expansions of `F` are
 * simply juxtaposed, so mapping over `a, b, c` yields `F(a…) F(b…) F(c…)` with
 * nothing in between. If you need a comma-separated list (an argument list, an
 * initializer, an enumerator list), `F` must emit the separator itself.
 *
 * @warning `MLIB_MAP_MACRO` **cannot be nested**: it will not expand within the
 * `Action` of another `MLIB_MAP_MACRO`. The outer map is still being expanded,
 * so the inner name is "painted blue" and left alone. This fails *silently*,
 * emitting the literal tokens `MLIB_MAP_MACRO(...)` into the translation unit
 * rather than producing a diagnostic. To nest a map, the inner `Action` must
 * invoke `_mlibMapMacro` instead; the outer `MLIB_EVAL` will then drive both:
 *
 *      // ✗ Bad: expands to literal "MLIB_MAP_MACRO(Inner, a, p, q) ..."
 *      #define OUTER(x, k, n) MLIB_MAP_MACRO(Inner, x, p, q)
 *      // ✓ Good:
 *      #define OUTER(x, k, n) _mlibMapMacro(Inner, x, p, q)
 *
 * If `xs` is empty, this expands to nothing — but the empty list must be spelled
 * as an explicit empty argument, with the trailing comma retained:
 *
 *      MLIB_MAP_MACRO(F, K, )   // ✓ Supported: `...` gets one empty argument
 *      MLIB_MAP_MACRO(F, K)     // ✗ Not supported: `...` gets no argument
 *
 * Both *ought* to expand to nothing, but C99 requires the `...` of a variadic
 * macro to receive at least one argument. An argument made of no preprocessing
 * tokens satisfies that; omitting the argument entirely does not, and GCC/Clang
 * diagnose it under `-pedantic`.
 *
 * `xs` may hold at most **63** items. That ceiling comes from `_mlibPick64th`
 * (see `mlib/pp/basic.h`) by way of `_mlibHasComma`. Exceeding it does not fail
 * cleanly: expect a stray pasting diagnostic from `MLIB_IS_EMPTY` followed by
 * unexpanded garbage in the output.
 */
#define MLIB_MAP_MACRO(Action, Constant, ...) MLIB_EVAL(_mlibMapMacro(Action, Constant, __VA_ARGS__))

// clang-format off
/**
 * @brief Like MLIB_MAP_MACRO, but does not force the recursive evaluation of
 * the mapping. Use this within other macro definitions that will already be
 * expanded with MLIB_EVAL
 *
 * This is also the *only* way to nest one map inside another: because it omits
 * the `MLIB_EVAL`, it is not painted blue by an enclosing `MLIB_MAP_MACRO` and
 * so the outer `MLIB_EVAL` is free to expand it. See the warning on
 * `MLIB_MAP_MACRO`.
 */
#define _mlibMapMacro(Action, Constant, ...) \
   /*-
    * If passed no arguments, we want to immediately expand to nothing at all.
    * Otherwise, go to the first step in the map expansion
    */ \
   MLIB_IF_ELSE(MLIB_IS_EMPTY(__VA_ARGS__)) \
      (MLIB_NOTHING) \
      (_mlibMapMacroFirst) \
   /*-
    * The MLIB_NOTHING used throughout these macros is a load-bearing trick, and
    * it is not about argument evaluation order.
    *
    * The MLIB_IF_ELSE above expands to a bare macro *name*, and the argument
    * list below is what that name is meant to consume. Placing MLIB_NOTHING()
    * between them separates the two, so the preprocessor does not see an
    * invocation during the current rescan: by the time the MLIB_NOTHING() is
    * removed, the scan has already moved past the name. The call therefore
    * happens on a *later* pass, driven by the enclosing MLIB_EVAL.
    *
    * That deferral is the whole engine. It is what lets MLIB_EVAL step the
    * recursion forward one iteration per pass, and it is what keeps a helper
    * from ever appearing within its own expansion (which the preprocessor would
    * refuse to expand again — "blue paint" — halting the map partway through).
    *
    * MLIB_DEFERRED() in mlib/pp/basic.h does exactly this, but it cannot be used
    * here: it takes the macro name as an argument, whereas the name we need to
    * defer is itself produced by expanding MLIB_IF_ELSE.
    */ \
   MLIB_NOTHING()(Action, Constant, __VA_ARGS__)

#define _mlibMapMacroFirst(Action, Constant, ...) \
   /*-
    * We know we have at least one arg, but detect whether we are passed more
    * than one argument. We want to avoid warnings about missing argument
    * lists for variadic macros. This protects against that by immediately
    * short-circuiting to the final expansion if-and-only-if we are passed a
    * single item to map.
    */ \
   MLIB_IF_ELSE(_mlibHasComma(__VA_ARGS__)) \
      (_mlibMapMacro_A) \
      (_mlibMapMacro_final) \
   /*-
    * Invoke the first expansion helper. Pass a zero to initialize the counter.
    */ \
   MLIB_NOTHING() (Action, Constant, 0, __VA_ARGS__)

/*-
 * MAP() will now bounce between two different expansion helpers. Since neither
 * one immediately expands to itself, it allows the EVAL to repeatedly expand
 * each of the below definitions into each other. The two are identical other
 * than their name.
 */
#define _mlibMapMacro_A(Action, Constant, Counter, Head, ...) \
   /*-
    * Do the actual expansion to the user's invocation. Pass the next list
    * item as the first argument, the user's constant as the second argument,
    * and the incrementing counter as the third.
    *
    * The counter is parenthesized to make it a primary expression, preventing
    * operator precedence issues if the user attempts to do arithmetic with it.
    */ \
   Action(Head, Constant, (Counter)) \
   /*-
    * Decide what to do next. If there is more than one argument remaining,
    * bounce to the other map expansion helper. If there is only one argument
    * remaining to expand, instead go to the final expansion helper.
    */ \
   MLIB_IF_ELSE(_mlibHasComma(__VA_ARGS__)) \
      (_mlibMapMacro_B) \
      (_mlibMapMacro_final) \
   MLIB_NOTHING() (Action, Constant, Counter + 1, __VA_ARGS__)

// The other half of the bounce. Identical to _mlibMapMacro_A above, including
// the reasoning in its comments, except that it hands back to _mlibMapMacro_A.
// Keep the two in sync.
#define _mlibMapMacro_B(Action, Constant, Counter, Head, ...) \
   Action(Head, Constant, (Counter)) \
   MLIB_IF_ELSE(_mlibHasComma(__VA_ARGS__)) \
      (_mlibMapMacro_A) \
      (_mlibMapMacro_final) \
   MLIB_NOTHING() (Action, Constant, Counter + 1, __VA_ARGS__)

#define _mlibMapMacro_final(Action, Constant, Counter, Final) \
   Action(Final, Constant, (Counter))

// clang-format on

#endif // MLIB_PP_MAP_H_INCLUDED
