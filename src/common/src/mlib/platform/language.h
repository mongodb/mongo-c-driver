/**
 * @file mlib/platform/language.h
 * @brief Compilation language detection
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

#ifndef MLIB_PLATFORM_LANGUAGE_H_INCLUDED
#define MLIB_PLATFORM_LANGUAGE_H_INCLUDED

#include <mlib/pp/basic.h>

/**
 * @brief Define the following language detection and conditional compilation
 * macros:
 *
 *  • mlib_is_cxx() → 0/1
 *  • mlib_is_not_cxx() → 0/1
 *  • MLIB_IF_CXX(<content>)
 *      → Expands to `<content>` if-and-only-if compiling as C++
 *  • MLIB_IF_NOT_CXX(<content>)
 *      → Expands to `<content>` if-and-only-if not compiling as C++ (e.g. as C)
 */
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

/**
 * @brief Expand to some content depending on the compilation language
 *
 *      MLIB_LANG_PICK (<c-language-content>) (<c++-language-content>)
 */
#define MLIB_LANG_PICK _mlibLangPick

#if mlib_is_cxx()
#define _mlibLangPick(...) /* Drop first argument list */ MLIB_JUST
#else
#define _mlibLangPick(...) __VA_ARGS__ MLIB_NOTHING /* Drop second argument list */
#endif

// clang-format off
/**
 * @brief Use as the prefix of a braced initializer within C headers, allowing
 * the initializer to appear as a compound-init in C and an equivalent braced
 * aggregate-init in C++
 */
#define mlib_init(T) \
   MLIB_LANG_PICK \
        ((T)) /* For C, wrap the type in parentheses to make a compound literal */ \
        ( T ) /* For C++, just place the type bare to make a braced initializer */
// clang-format on

/**
 * @brief Expands to `noexcept` when compiled as C++, otherwise expands to
 * nothing
 */
#define mlib_noexcept MLIB_IF_CXX(noexcept)

#endif // MLIB_PLATFORM_LANGUAGE_H_INCLUDED
