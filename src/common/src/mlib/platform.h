/**
 * @file mlib/platform.h
 * @brief Operating System Headers and Definitions
 * @date 2025-04-21
 *
 * This file will conditionally include the general system headers available
 * for the current host platform.
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

#ifndef MLIB_PLATFORM_H_INCLUDED
#define MLIB_PLATFORM_H_INCLUDED

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// Feature detection
#ifdef __has_include
#if __has_include(<features.h>)
#include <features.h>
#endif
#endif

#endif // MLIB_PLATFORM_H_INCLUDED
