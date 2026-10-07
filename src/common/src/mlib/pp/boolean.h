/**
 * @file mlib/pp/boolean.h
 * @brief Preprocessor boolean-handling macros
 * @date 2025-07-29
 *
 * These macros are useful for creating/evaluating boolean-style conditions
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

#ifndef MLIB_PP_BOOLEAN_H_INCLUDED
#define MLIB_PP_BOOLEAN_H_INCLUDED

#include "./basic.h" // paste

/**
 * @brief Normalize a preprocessor condition to a literal `0` or `1` token
 *
 * @param Cond The condition must be one of `1`, `true`, `0`, `false`, or empty.
 * Any other argument generates a compilation error.
 *
 * @note Avoid using `true` or `false` identifiers, as some C versions expand
 * these to non-trivial macros of type `(_Bool)`, and a required token-pasting
 * will fail.
 *
 * @note This is deliberately declared as a variadic macro even though it accepts
 * only a single condition. A single *named* parameter would make the legal
 * empty-condition spelling `MLIB_BOOLEAN()` look like a call with zero arguments
 * to the old (non-conforming) MSVC preprocessor, which then emits warning C4003
 * ("not enough arguments for function-like macro invocation"). A variadic
 * parameter treats `()` as a well-formed empty argument list, so the empty
 * spelling is warning-free everywhere. The `Cond` should still be a single token
 * (or empty); passing a top-level comma is a usage error.
 *
 * Troubleshooting:
 *
 * • If you see "pasting formed … an invalid preprocessing token", it indicates
 *   that the leading token in `Cond` is not a valid identifier token:
 *
 *   • If you passed `true` or `false` and you are compiling in C99 mode, it
 *     indicates that your standard library has #define'd `true` or `false` to
 *     some non-trivial expression of type `_Bool`.
 *   • If your argument begins with an operator or punctuation (including an
 *     opening parenthesis), you will also see the above error.
 *
 * • If you see an error naming an undeclared identifier `_mlibBooleanNormalize_<x>`
 *   (or a warning about an implicit declaration of that function), it means you
 *   passed an ordinary identifier `<x>` that is not one of the recognized
 *   spellings. This is the most common mistake, and note that it is *not*
 *   diagnosed by the preprocessor: the paste succeeds and produces a name that
 *   is simply never defined, so the error surfaces later as a C/C++ error at the
 *   point of use, with no indication that a preprocessor condition was at fault.
 */
#define MLIB_BOOLEAN(...)                                                        \
   /*-                                                                           \
    * Check the answer by token-pasting with a prefix to form one of a fixed set \
    * of function-like macro names that will expand to either `0` or `1`.        \
    *                                                                            \
    * The trailing `()` invokes that name. The normalization forms are           \
    * deliberately function-like rather than object-like: the empty-`Cond`       \
    * case requires the bare name `_mlibBooleanNormalize_`. If object-like,      \
    * that prefix would expand during `MLIB_PASTE`'s argument pre-expansion,     \
    * before it could be pasted with a non-empty condition. Keeping it           \
    * function-like leaves the prefix inert until the pasted name is invoked.    \
    */                                                                           \
   MLIB_PASTE(_mlibBooleanNormalize_, __VA_ARGS__)()
// The following normalization forms are the complete set of spellings accepted
// by MLIB_BOOLEAN. Adding new forms here will support additional spellings.
// Each must be function-like and take no arguments; see the note above.
// Simple 0/1:
#define _mlibBooleanNormalize_1() 1
#define _mlibBooleanNormalize_0() 0
// Detect `true` or `false` (not recommended in C99)
#define _mlibBooleanNormalize_true() 1
#define _mlibBooleanNormalize_false() 0
// Formed if `Cond` is an empty token
#define _mlibBooleanNormalize_() 0

/**
 * @brief Negate the given argument list as a boolean expression
 *
 * @param Cond The condition expression. Should be `1`, `0`, `true`, `false`, or
 * empty. If given anything else, generates a compilation error
 *
 * Expands to `0` if the condition is truthy, otherwise `1`.
 *
 * @note Variadic for the same reason as `MLIB_BOOLEAN`: to keep the empty
 * spelling `MLIB_NEGATE()` from tripping MSVC's C4003 warning.
 *
 * Troubleshooting: Refer to the troubleshooting section on `MLIB_BOOLEAN`
 */
#define MLIB_NEGATE(...)                                                            \
   /*-                                                                              \
    * Like MLIB_BOOLEAN, but uses a prefix that just swaps zero and one literals.   \
    * First normalizes to 0/1 using MLIB_BOOLEAN, so we can rely on the same set of \
    * boolean word detection.                                                       \
    */                                                                              \
   MLIB_PASTE(_mlibBooleanNegation_, MLIB_BOOLEAN(__VA_ARGS__))()
// The negation swappers:
#define _mlibBooleanNegation_1() 0
#define _mlibBooleanNegation_0() 1

/**
 * @brief Evaluate the logical disjunction of two boolean conditions.
 *
 * @param A The left-hand operand
 * @param B The right-hand operand
 *
 * The two boolean condition arguments are normalized through `MLIB_BOOLEAN`
 *
 * Troubleshooting: Refer to "troubleshooting" on MLIB_BOOLEAN
 */
#define MLIB_OR(A, B) MLIB_PASTE_3(_mlibBooleanDisjunction_, MLIB_BOOLEAN(A), MLIB_BOOLEAN(B))()
// "or" truth table:
#define _mlibBooleanDisjunction_11() 1
#define _mlibBooleanDisjunction_10() 1
#define _mlibBooleanDisjunction_01() 1
#define _mlibBooleanDisjunction_00() 0

/**
 * @brief Evaluate the logical conjunction of two boolean conditions.
 *
 * @param A The left-hand operand
 * @param B The right-hand operand
 *
 * The two boolean condition arguments are normalized through `MLIB_BOOLEAN`
 *
 * Troubleshooting: Refer to "troubleshooting" on MLIB_BOOLEAN
 */
#define MLIB_AND(A, B) MLIB_PASTE_3(_mlibBooleanConjunction_, MLIB_BOOLEAN(A), MLIB_BOOLEAN(B))()
// "and" truth table:
#define _mlibBooleanConjunction_11() 1
#define _mlibBooleanConjunction_10() 0
#define _mlibBooleanConjunction_01() 0
#define _mlibBooleanConjunction_00() 0

/**
 * @brief Evaluate the logical exclusive disjunction of two boolean conditions.
 *
 * @param A The left-hand operand
 * @param B The right-hand operand
 *
 * The two boolean condition arguments are normalized through `MLIB_BOOLEAN`
 *
 * Troubleshooting: Refer to "troubleshooting" on MLIB_BOOLEAN
 */
#define MLIB_XOR(A, B) MLIB_PASTE_3(_mlibBooleanExclusiveDisjunction_, MLIB_BOOLEAN(A), MLIB_BOOLEAN(B))()
// "xor" truth table:
#define _mlibBooleanExclusiveDisjunction_11() 0
#define _mlibBooleanExclusiveDisjunction_10() 1
#define _mlibBooleanExclusiveDisjunction_01() 1
#define _mlibBooleanExclusiveDisjunction_00() 0

#endif // MLIB_PP_BOOLEAN_H_INCLUDED
