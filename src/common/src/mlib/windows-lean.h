/**
 * @file windows-lean.h
 * @brief Windows.h inclusion shim
 * @date 2025-04-07
 *
 * This file will conditionally include `<windows.h>`, and wraps it with
 * `WIN32_LEAN_AND_MEAN` and `NOMINMAX`.
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
#ifndef MLIB_WINDOWS_LEAN_H_INCLUDED
#define MLIB_WINDOWS_LEAN_H_INCLUDED

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#endif // MLIB_WINDOWS_LEAN_H_INCLUDED
