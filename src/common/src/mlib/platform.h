/**
 * @file mlib/platform.h
 * @brief Operating System Headers and Definitions
 * @date 2025-04-21
 *
 * This file will conditionally include the general system headers available
 * for the current host platform.
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

#ifndef MLIB_PLATFORM_H_INCLUDED
#define MLIB_PLATFORM_H_INCLUDED

// clang-format off

// Windows headers
#ifdef _WIN32
    // Check that our WINNT version isn't too old to be used
    #if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x601)
        #undef _WIN32_WINNT
    #endif
    #ifndef _WIN32_WINNT
        // Request a new-enough version of the Win32 API (required for MinGW)
        #define _WIN32_WINNT 0x601
    #endif
    #define NOMINMAX
    // Winsock must be included before windows.h
    #include <winsock2.h>
    #include <windows.h>
#endif

// POSIX headers
#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/types.h>
#endif

// Feature detection
#ifdef __has_include
    #if __has_include(<features.h>)
        #include <features.h>
    #endif
#endif

// clang-format on

#endif // MLIB_PLATFORM_H_INCLUDED
