/**
 * @file mlib/pp/is-empty.h
 * @brief Empty preprocessor argument detection
 * @date 2025-07-29
 *
 * This presents macros that can detect whether they are invoked with any arguments
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
#ifndef MLIB_PP_IS_EMPTY_H_INCLUDED
#define MLIB_PP_IS_EMPTY_H_INCLUDED

#include "./basic.h"   // paste, nothing, first_arg, deferred, pick64th
#include "./boolean.h" // negate

// clang-format off

/**
 * @brief Expand to 1 if given no arguments, otherwise 0.
 *
 * This could be done trivially using __VA_OPT__, but we need to work on
 * older compilers. This non-trivial version can fail in some unusual scenarios,
 * so the trivial version is preferred in the future when we can use __VA_OPT__
 *
 * Troubleshooting: This macro expansion can produce a compilation error/warning
 * if the final token in `...` is the name of a macro that expects more than one
 * argument.
 */
#define MLIB_IS_EMPTY(...) \
    _mlibIsEmptyImpl( \
        /* Expands to '1' if __VA_ARGS__ contains any top-level commas */ \
        _mlibHasComma(__VA_ARGS__), \
        /*-
         * Expands to '1' if __VA_ARGS__ begins with a parenthesis, because
         * that will cause an "invocation" of _mlibCommaIfParens,
         * which immediately expands to a single comma.
         */ \
        _mlibHasComma(_mlibCommaIfParens __VA_ARGS__), \
        /*-
         * Expands to '1' if __VA_ARGS__ expands to a function-like macro name
         * that then expands to anything containing a top-level comma. This
         * is required to prevent false detection of the next condition below.
         *
         * Of the four conditions, this one is troublesome, in that it can produce
         * a macro expansion `foo()`, where `foo` might be a macro name that expects
         * more than a single argument.
         */ \
        _mlibHasComma(__VA_ARGS__()), \
        /*-
         * Expands to '1' if __VA_ARGS__ expands to nothing.
         *
         * If `__VA_ARGS__` expands to something very goofy and weird, this may also
         * expand to `1`, but that case is guarded by the other three tests above.
         */ \
        _mlibHasComma(_mlibCommaIfParens __VA_ARGS__ ()))
/*-
 * A helper for IS_EMPTY(): If given (0, 0, 0, 1), expands as:
 *    - first: _mlibHasComma(_mlibIsEmptyCase_0001)
 *    -  then: _mlibHasComma(,)
 *    -  then: 1
 * Given any other arguments:
 *    - first: _mlibHasComma(_mlibIsEmptyCase_<somethingelse>)
 *    -  then: 0
 */
#define _mlibIsEmptyImpl(_1, _2, _3, _4) \
    _mlibHasComma(MLIB_PASTE_5(_mlibIsEmptyCase_, _1, _2, _3, _4))
#define _mlibIsEmptyCase_0001 ,

// The trivial version based on va-opt:
// ! #define MLIB_IS_EMPTY(...) MLIB_FIRST_ARG(__VA_OPT__(0, ) 1)

/**
 * @brief Expands to `1` if given any arguments, otherwise expands to `0`
 */
#define MLIB_IS_NOT_EMPTY(...) MLIB_NEGATE(MLIB_IS_EMPTY(__VA_ARGS__))

/**
 * @brief Conditionally expands to some content if-and-only-if the first argument
 * list is not empty. Requires two argument lists:
 *
 *  MLIB_IF_NOT_EMPTY (<args>) (<content>)
 *
 * The macro will expand to `<content>` if-and-only-if `<args>` is not empty.
 * If `<args>` is empty, then this macro expands to nothing.
 */
#define MLIB_IF_NOT_EMPTY(...) MLIB_PASTE(_mlibIfNotEmpty_, MLIB_IS_EMPTY(__VA_ARGS__))
#define _mlibIfNotEmpty_1(...) // Arglist is empty: Expand to nothing
#define _mlibIfNotEmpty_0(...) __VA_ARGS__

/**
 * @brief Expands to a single comma if-and-only-if the given argument list is not empty
 *
 * Useful to emulate `__VA_OPT__(,)` or GCC `##__VA_ARGS__` extension.
 */
#define MLIB_OPT_COMMA(...) MLIB_IF_NOT_EMPTY(__VA_ARGS__)(,)

/**
 * @brief Expands to `1` if the given macro argument is a parenthesized group of
 *    tokens, otherwise `0`.
 *
 * This only detects if the first token is an opening parenthesis. Trailing tokens
 * are not considered.
 */
#define MLIB_IS_PARENTHESIZED(X) _mlibHasComma(_mlibCommaIfParens X)

/*-
 * Expands to `1` if the argument list contains a top-level comma
 *
 * Note that this macro will fail if you pass *too many* top-level commas.
 */
#define _mlibHasComma(...) \
   _mlibHasComma1( \
        /*-
         * We defer the expansion of Pick64th because MSVC's old broken
         * preprocessor expands __VA_ARGS__ at the wrong time, by forcing
         * __VA_ARGS__ to expand before we try to expand `_mlibPick64th`.
         * The `_mlibHasComma1` forces another expansion pass after the
         * `MLIB_DEFERRED` expands.
         */ \
        MLIB_DEFERRED(_mlibPick64th) \
        /*-
         * If va-args contains any top-level comma, then all arguments will
         * be shifted to the right, and a `1` will land in the 64th
         * position. Otherwise, the `0` will be the 64th. The final tilde is
         * used to satisfy requirements around variadic macro argument lists for
         * _mlibPick64th.
         */ \
        (__VA_ARGS__, \
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, ~))
#define _mlibHasComma1(...) __VA_ARGS__

/*-
 * Expands to a single comma if invoked as a function-like macro
 *
 * Used by is-empty to detect if the following tokens are parentheses
 */
#define _mlibCommaIfParens(...) ,

// NOTE: `_mlibPick64th`, used above by `_mlibHasComma`, is defined in
// `mlib/pp/basic.h`. It is shared with `MLIB_ARG_COUNT` in `mlib/pp/args.h`,
// so it lives with the other primitives rather than here.

#endif // MLIB_PP_IS_EMPTY_H_INCLUDED
