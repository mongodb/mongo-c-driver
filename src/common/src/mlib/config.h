/**
 * @file config.h
 * @brief Provides utility macros
 * @date 2024-08-29
 *
 * @copyright Copyright (c) 2025
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
#pragma once

#ifndef _WIN32
#include <sys/param.h> // Endian detection
#endif

/**
 * @brief A function-like macro that always expands to nothing
 */
#define MLIB_NOTHING(...)

/**
 * @brief A function macro that simply expands to its arguments unchanged
 */
#define MLIB_JUST(...) __VA_ARGS__

// Paste two tokens
#ifndef _MSC_VER
#define MLIB_PASTE(A, ...) _mlibPaste1 (A, __VA_ARGS__)
#else
#define MLIB_PASTE(A, ...) MLIB_JUST (_mlibPaste1 (A, __VA_ARGS__))
#endif
// Paste three tokens
#define MLIB_PASTE_3(A, B, ...) MLIB_PASTE (A, MLIB_PASTE (B, __VA_ARGS__))
// Paste four tokens
#define MLIB_PASTE_4(A, B, C, ...) MLIB_PASTE (A, MLIB_PASTE_3 (B, C, __VA_ARGS__))
// Paste five tokens
#define MLIB_PASTE_5(A, B, C, D, ...) MLIB_PASTE (A, MLIB_PASTE_4 (B, C, D, __VA_ARGS__))
#define _mlibPaste1(A, ...) A##__VA_ARGS__

/**
 * @brief Convert the token sequence into a string after macro expansion
 */
#define MLIB_STR(...) _mlibStr (__VA_ARGS__)
#define _mlibStr(...) #__VA_ARGS__

#define MLIB_EVAL_32(...) MLIB_EVAL_16 (MLIB_EVAL_16 (__VA_ARGS__))
#define MLIB_EVAL_16(...) MLIB_EVAL_8 (MLIB_EVAL_8 (__VA_ARGS__))
#define MLIB_EVAL_8(...) MLIB_EVAL_4 (MLIB_EVAL_4 (__VA_ARGS__))
#define MLIB_EVAL_4(...) MLIB_EVAL_2 (MLIB_EVAL_2 (__VA_ARGS__))
#define MLIB_EVAL_2(...) MLIB_EVAL_1 (MLIB_EVAL_1 (__VA_ARGS__))
#define MLIB_EVAL_1(...) __VA_ARGS__

// clang-format off
/**
 * @brief Expand to 1 if given no arguments, otherwise 0.
 *
 * This could be done trivially using __VA_OPT__, but we need to work on
 * older compilers.
 */
#define MLIB_IS_EMPTY(...) \
    _mlibIsEmpty_1( \
        /* Expands to '1' if __VA_ARGS__ contains any top-level commas */ \
        _mlibHasComma(__VA_ARGS__), \
        /* Expands to '1' if __VA_ARGS__ begins with a parenthesis, because \
         * that will cause an "invocation" of _mlibCommaIfParens, \
         * which immediately expands to a single comma. */ \
        _mlibHasComma(_mlibCommaIfParens __VA_ARGS__), \
        /* Expands to '1' if __VA_ARGS__ expands to a function-like macro name \
         * that then expands to anything containing a top-level comma */ \
        _mlibHasComma(__VA_ARGS__ ()), \
        /* Expands to '1' if __VA_ARGS__ expands to nothing. */ \
        _mlibHasComma(_mlibCommaIfParens __VA_ARGS__ ()))
// Expand to 1 if the argument list has a comma. The weird definition is to support
// old MSVC's bad preprocessor
#define _mlibHasComma(...) \
   MLIB_JUST(_mlibPickSixteenth \
               MLIB_NOTHING("MSVC workaround") \
            (__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, ~))
#define _mlibCommaIfParens(...) ,

/**
 * A helper for isEmpty(): If given (0, 0, 0, 1), expands as:
 *    - first: _mlibHasComma(_mlibIsEmptyCase_0001)
 *    -  then: _mlibHasComma(,)
 *    -  then: 1
 * Given any other aruments:
 *    - first: _mlibHasComma(_bsonDSL_isEmpty_CASE_<somethingelse>)
 *    -  then: 0
 */
#define _mlibIsEmpty_1(_1, _2, _3, _4) \
    _mlibHasComma(MLIB_PASTE_5(_mlibIsEmptyCase_, _1, _2, _3, _4))
#define _mlibIsEmptyCase_0001 ,

#define MLIB_IS_NOT_EMPTY(...) MLIB_PASTE (_mlibNotEmpty_, MLIB_IS_EMPTY (__VA_ARGS__))
#define _mlibNotEmpty_1 0
#define _mlibNotEmpty_0 1
// clang-format on

/**
 * @brief If the argument expands to `0`, `false`, or nothing, expands to `0`.
 * Otherwise expands to `1`.
 */
#define MLIB_BOOLEAN(...) MLIB_IS_NOT_EMPTY (MLIB_PASTE_3 (_mlib, Bool_, __VA_ARGS__))
#define _mlibBool_0
#define _mlibBool_false
#define _mlibBool_

/**
 * @brief A ternary macro. Expects three parenthesized argument lists in
 * sequence.
 *
 * If the first argument list is a truthy value, expands to the second argument
 * list. Otherwise, expands to the third argument list. The unused argument list
 * is not expanded and is discarded.
 */
#define MLIB_IF_ELSE(...) MLIB_NOTHING (#__VA_ARGS__) MLIB_PASTE (_mlibIfElseBranch_, MLIB_BOOLEAN (__VA_ARGS__))
#define _mlibIfElseBranch_1(...) __VA_ARGS__ _mlibNoExpandNothing
#define _mlibIfElseBranch_0(...) MLIB_NOTHING (#__VA_ARGS__) MLIB_JUST
#define _mlibNoExpandNothing(...) MLIB_NOTHING (#__VA_ARGS__)

/**
 * @brief Expands to an integer literal corresponding to the number of macro
 * arguments. Supports up to fifteen arguments.
 */
#define MLIB_ARG_COUNT(...)                   \
   MLIB_IF_ELSE (MLIB_IS_EMPTY (__VA_ARGS__)) \
   (0) (_mlibPickSixteenth (__VA_ARGS__, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#define _mlibPickSixteenth(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, ...) _16

/**
 * @brief Expand to a call expression `Prefix##_argc_N(...)`, where `N` is the
 * number of macro arguments.
 */
#define MLIB_ARGC_PICK(Prefix, ...) \
   MLIB_JUST (MLIB_PASTE_3 (Prefix, _argc_, MLIB_ARG_COUNT (__VA_ARGS__)) (__VA_ARGS__))

#ifdef __cplusplus
#define mlib_is_cxx() 1
#define mlib_is_not_cxx() 0
#define MLIB_IF_CXX(...) __VA_ARGS__
#define MLIB_IF_NOT_CXX(...)
#else
#define mlib_is_cxx() 0
#define mlib_is_not_cxx() 1
#define MLIB_IF_CXX(...)
#define MLIB_IF_NOT_CXX(...) __VA_ARGS__
#endif

#define MLIB_LANG_PICK MLIB_IF_ELSE (mlib_is_not_cxx ())

/**
 * @brief Expands to `noexcept` when compiled as C++, otherwise expands to
 * nothing
 */
#define mlib_noexcept MLIB_IF_CXX (noexcept)

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#define mlib_is_little_endian() (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#elif defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN)
#define mlib_is_little_endian() (__BYTE_ORDER == __LITTLE_ENDIAN)
#elif defined(_WIN32)
#define mlib_is_mlib_is_little_endian() 1
#else
#error "Do not know how to detect endianness on this platform."
#endif

// clang-format off
/**
 * @brief Expands to a static assertion declaration.
 *
 * When supported, this can be replaced with `_Static_assert` or `static_assert`
 */
#define mlib_static_assert(...) MLIB_ARGC_PICK (_mlib_static_assert, __VA_ARGS__)
#define _mlib_static_assert_argc_1(Expr) \
   _mlib_static_assert_argc_2 ((Expr), "Static assertion failed")
#define _mlib_static_assert_argc_2(Expr, Msg) \
   extern int \
   MLIB_PASTE (_mlib_static_assert_placeholder, __COUNTER__)[(Expr) ? 2 : -1] \
   MLIB_IF_GNU_LIKE (__attribute__ ((unused)))
// clang-format on

#define mlib_extern_c_begin() MLIB_IF_CXX(extern "C" {) mlib_static_assert(1, "")
#define mlib_extern_c_end() MLIB_IF_CXX( \
   }) mlib_static_assert(1, "")

#ifdef __GNUC__
#define mlib_is_gnu_like() 1
#ifdef __clang__
#define mlib_is_gcc() 0
#define mlib_is_clang() 1
#else
#define mlib_is_gcc() 1
#define mlib_is_clang() 0
#endif
#define mlib_is_msvc() 0
#elif defined(_MSC_VER)
#define mlib_is_gnu_like() 0
#define mlib_is_clang() 0
#define mlib_is_gcc() 0
#define mlib_is_msvc() 1
#endif

#if defined(_WIN32)
#define mlib_is_win32() 1
#define mlib_is_unix() 0
#else
#define mlib_is_unix() 1
#define mlib_is_win32() 0
#endif

#define MLIB_IF_CLANG(...) MLIB_IF_ELSE (mlib_is_clang ()) (__VA_ARGS__) (MLIB_NOTHING (#__VA_ARGS__))
#define MLIB_IF_GCC(...) MLIB_IF_ELSE (mlib_is_gcc ()) (__VA_ARGS__) (MLIB_NOTHING (#__VA_ARGS__))
#define MLIB_IF_GNU_LIKE(...) MLIB_IF_GCC (__VA_ARGS__) MLIB_IF_CLANG (__VA_ARGS__) MLIB_NOTHING (#__VA_ARGS__)
#define MLIB_IF_UNIX_LIKE(...) MLIB_IF_ELSE (mlib_is_unix ()) (__VA_ARGS__) (MLIB_NOTHING (#__VA_ARGS__))

// note: Bug on GCC preprocessor prevents us from using if/else trick to omit MSVC code
#if mlib_is_msvc()
#define MLIB_IF_MSVC(...) __VA_ARGS__
#define mlib_pragma(...) __pragma (__VA_ARGS__) mlib_static_assert (1, "")
#else
#define MLIB_IF_MSVC(...) MLIB_NOTHING (#__VA_ARGS__)
#define mlib_pragma(...) _Pragma (#__VA_ARGS__) mlib_static_assert (1, "")
#endif

#define mlib_diagnostic_push()                           \
   MLIB_IF_GNU_LIKE (mlib_pragma (GCC diagnostic push);) \
   MLIB_IF_MSVC (mlib_pragma (warning (push));)          \
   mlib_static_assert (true, "")

#define mlib_diagnostic_pop()                           \
   MLIB_IF_GNU_LIKE (mlib_pragma (GCC diagnostic pop);) \
   MLIB_IF_MSVC (mlib_pragma (warning (pop));)          \
   mlib_static_assert (true, "")

#define mlib_gcc_warning_disable(Warning)                      \
   MLIB_IF_GCC (mlib_pragma (GCC diagnostic ignored Warning);) \
   mlib_static_assert (true, "")

#define mlib_gnu_warning_disable(Warning)                           \
   MLIB_IF_GNU_LIKE (mlib_pragma (GCC diagnostic ignored Warning);) \
   mlib_static_assert (true, "")

#define mlib_msvc_warning(...)                         \
   MLIB_IF_MSVC (mlib_pragma (warning (__VA_ARGS__));) \
   mlib_static_assert (true, "")
