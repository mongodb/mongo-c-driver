/**
 * @file mlib/pp/args.h
 * @brief Preprocessor macros for working with variadic argument lists
 * @date 2025-07-29
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
 *
 */

#ifndef MLIB_PP_ARGS_H_INCLUDED
#define MLIB_PP_ARGS_H_INCLUDED

#include "./basic.h"    // paste, just, pick64th
#include "./if-else.h"  // if_else
#include "./is-empty.h" // is_empty

// clang-format off

/**
 * @brief Expands to an integer literal corresponding to the number of macro
 * arguments. Supports up to 63 arguments.
 *
 * This may be replaced someday in the future with a possible __VA_NARG__()
 * preprocessor intrinsic.
 *
 * @note The preprocessor only considers parentheses for token grouping, meaning
 * that braces {} or square brackets [] will not "group" a token sequence that
 * contains a comma. This can cause surprising results if the macro is invoked
 * with a compound literal or braced initializer. To avoid this, compound literals
 * should be wrapped in parentheses for such function-like macros.
 */
#define MLIB_ARG_COUNT(...) \
    MLIB_IF_ELSE(MLIB_IS_EMPTY(__VA_ARGS__)) \
        (0) \
        (_mlibPick64th( \
            /*-
            * Expand the argument list. For every top-level comma, shifts the
            * argument list over by 1.
            */ \
            __VA_ARGS__, \
            /*-
            * Argument counts, up to 63. For each top-level comma in __VA_ARGS__,
            * these are shifted along, with `0` in the 64th position to start.
            * To support more than 63 arguments, more integers must be added,
            * and `_mlibPick64th` needs to be extended to a higher number.
            */ \
            63, 62, 61, 60, 59, 58, 57, 56, 55, 54, \
            53, 52, 51, 50, 49, 48, 47, 46, 45, 44, \
            43, 42, 41, 40, 39, 38, 37, 36, 35, 34, \
            33, 32, 31, 30, 29, 28, 27, 26, 25, 24, \
            23, 22, 21, 20, 19, 18, 17, 16, 15, 14, \
            13, 12, 11, 10,  9,  8,  7,  6,  5,  4, \
            3,  2,  1,  0, ~))

/**
 * @brief Expand to a different function-like macro invocation based on
 * the number of arguments passed to the macro:
 *
 *      MLIB_ARGC_PICK(<prefix>, <args>)
 *
 * Where `<prefix>` is an arbitrary identifier and `<args>` is zero or more
 * arguments. When given `<N>` argments in `<args>`, this will expand to the
 * following:
 *
 *      <prefix>_argc_<N>(<args>)
 *
 * Note that `<N>` may be zero: invoking with an empty `<args>` expands to
 * `<prefix>_argc_0()`, so a caller that wants to accept zero arguments must
 * define a `<prefix>_argc_0` overload. If it is not defined, the error appears
 * at the point of use as an unknown identifier rather than as a preprocessor
 * diagnostic.
 *
 * @warning The preprocessor only counts parentheses when looking at token
 * grouping. This means that things like C99 compound literals will count as
 * more than one argument if they have more than one initializer (and thus
 * contain a comma). Compound literals used with a macro like this requires that
 * the compound literal be wrapped in parentheses in order to count as a single
 * argument:
 *
 *      MLIB_ARGC_PICK(foo,  (my_type){1, 2, 3} ) // ← Expands to foo_argc_3(...)
 *      MLIB_ARGC_PICK(foo, ((my_type){1, 2, 3})) // ← Expands to foo_argc_1(...)
 *
 * XXX: The `MLIB_JUST` forces an additional expansion pass that works around a
 * bug in the old MSVC preprocessor, but is not required in a conforming preprocessor.
 */
#define MLIB_ARGC_PICK(Prefix, ...) MLIB_JUST(MLIB_ARGC_PASTE(Prefix, __VA_ARGS__)(__VA_ARGS__))

/**
 * @brief Perform the token-paste done by `MLIB_ARGC_PICK` without invoking the
 * result as a function-like macro.
 *
 * This is useful to control the expansion order of variadic macros used with
 * `ARGC_PICK`
 */
#define MLIB_ARGC_PASTE(Prefix, ...) \
    MLIB_PASTE_3(Prefix, _argc_, MLIB_ARG_COUNT(__VA_ARGS__))

#endif // MLIB_PP_ARGS_H_INCLUDED
