/**
 * @file mlib/platform/compiler.h
 * @brief Compiler detection macros
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
#ifndef MLIB_PLATFORM_COMPILER_H_INCLUDED
#define MLIB_PLATFORM_COMPILER_H_INCLUDED

#include <mlib/pp/basic.h>
#include <mlib/pp/if-else.h>

/**
 * @brief Define the following function-like macros:
 *
 * • mlib_is_gnu_like()
 *      → Expands to `1` if compiling with GCC or Clang, otherwise `0`
 * • mlib_is_gcc() → 0/1
 * • mlib_is_clang() → 0/1
 * • mlib_is_msvc() → 0/1
 */
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

/// @brief Expand the content if-and-only-if compiling with Clang
#define MLIB_IF_CLANG(...) MLIB_IF(mlib_is_clang())(__VA_ARGS__)
/// @brief Expand the content if-and-only-if compiling with GCC
#define MLIB_IF_GCC(...) MLIB_IF(mlib_is_gcc())(__VA_ARGS__)
/// @brief Expand the content if-and-only-if compiling with MSVC
#define MLIB_IF_MSVC(...) MLIB_IF(mlib_is_msvc())(__VA_ARGS__)
/// @brief Expand the content if-and-only-if compiling with GCC or Clang
#define MLIB_IF_GNU_LIKE(...) MLIB_IF(mlib_is_gnu_like())(__VA_ARGS__)

#endif // MLIB_PLATFORM_COMPILER_H_INCLUDED
