/**
 * @file mlib/platform/endian.h
 * @brief Integer endianness platform detection
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

#ifndef MLIB_PLATFORM_ENDIAN_H_INCLUDED
#define MLIB_PLATFORM_ENDIAN_H_INCLUDED

#ifndef _WIN32
#include <sys/param.h> // Endian detection on POSIX
#endif

/**
 * @brief Expands to 1 if-and-only-if we are compiling on a little-endian platform
 */
#define mlib_is_little_endian() _mlibIsLittleEndian()

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#define _mlibIsLittleEndian() (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#elif defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN)
#define _mlibIsLittleEndian() (__BYTE_ORDER == __LITTLE_ENDIAN)
#elif defined(_WIN32)
#define _mlibIsLittleEndian() 1
#else
#error "Do not know how to detect endianness on this platform."
#endif

#endif // MLIB_PLATFORM_ENDIAN_H_INCLUDED
