/**
 * @file mlib/static_assert.h
 * @brief A static_assertion compatibility macro
 * @date 2025-10-31
 *
 * This file defined `mlib_static_assert`, a function-like macro that expands
 * to a declaration that acts as a compile-time static assertion
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
#ifndef MLIB_STATIC_ASSERT_H_INCLUDED
#define MLIB_STATIC_ASSERT_H_INCLUDED

#include <mlib/platform/compiler.h>
#include <mlib/platform/language.h>
#include <mlib/pp/args.h>

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

#endif // MLIB_STATIC_ASSERT_H_INCLUDED
