/**
 * @file mlib/platform/os.h
 * @brief Operating system detection
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

#ifndef MLIB_PLATFORM_OS_H_INCLUDED
#define MLIB_PLATFORM_OS_H_INCLUDED

#include <mlib/pp/if-else.h>

#if defined(_WIN32)
#define mlib_is_win32() 1
#define mlib_is_unix() 0
#else
#define mlib_is_unix() 1
#define mlib_is_win32() 0
#endif

#define MLIB_IF_UNIX_LIKE(...) MLIB_IF(mlib_is_unix())(__VA_ARGS__)
#define MLIB_IF_WIN32(...) MLIB_IF(mlib_is_win32())(__VA_ARGS__)

#endif // MLIB_PLATFORM_OS_H_INCLUDED
