/**
 * @file mlib/loop.h
 * @brief Looping utility macros
 * @date 2025-01-29
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

#include <mlib/config.h>

#include <stdint.h> // u/intmax_t

/**
 * @brief Begin a loop over a range of integer values. Supports:
 *
 * - `mlib_foreach_{u,i}range(Var, Stop)`
 * - `mlib_foreach_{u,i}range(Var, Start, Stop)`
 *
 * If ommitted, starts at zero. The loop does not include the `Stop` value. The `Var`
 * variable cannot be modified within the loop. The loop variable is declared as the maximum
 * precision type for the requested signedness.
 */
#define mlib_foreach_urange(...) MLIB_ARGC_PICK (_mlib_foreach_urange, __VA_ARGS__)
#define mlib_foreach_irange(...) MLIB_ARGC_PICK (_mlib_foreach_irange, __VA_ARGS__)
#define _mlib_foreach_urange_argc_2(VarName, Stop) _mlib_foreach_urange_argc_3 (VarName, 0, Stop)
#define _mlib_foreach_urange_argc_3(VarName, Start, Stop) \
   _mlibForeachRange (uintmax_t,                          \
                      VarName,                            \
                      Start,                              \
                      Stop,                               \
                      MLIB_PASTE (VarName, _start),       \
                      MLIB_PASTE (VarName, _stop),        \
                      MLIB_PASTE (VarName, _counter),     \
                      MLIB_PASTE (VarName, _didbreak),    \
                      MLIB_PASTE (VarName, _once),        \
                      MLIB_PASTE (VarName, _done))
#define _mlib_foreach_irange_argc_2(VarName, Stop) _mlib_foreach_irange_argc_3 (VarName, 0, Stop)
#define _mlib_foreach_irange_argc_3(VarName, Start, Stop) \
   _mlibForeachRange (intmax_t,                           \
                      VarName,                            \
                      Start,                              \
                      Stop,                               \
                      MLIB_PASTE (VarName, _start),       \
                      MLIB_PASTE (VarName, _stop),        \
                      MLIB_PASTE (VarName, _counter),     \
                      MLIB_PASTE (VarName, _didbreak),    \
                      MLIB_PASTE (VarName, _once),        \
                      MLIB_PASTE (VarName, _done))

// clang-format off
#define _mlibForeachRange(VarType, VarName, StartValue, StopValue, StartVar, StopVar, Counter, DidBreak, Once, IsDone) \
    /* Loop stop condition */ \
    for (bool IsDone = false; !IsDone;) \
    /* Track if the user broke out of the inner loop */ \
    for (bool DidBreak = false; !IsDone;) \
    /* Capture the starting and stopping value first */ \
    for (VarType StartVar = (StartValue), StopVar = (StopValue); !IsDone;) \
    /* Declare the inner counter variable, and stop when we reach the stop value */ \
    for (VarType Counter = (StartVar); !(IsDone |= !(Counter < StopVar)); ++Counter) \
    /* `break` detection: */ \
    for (bool Once = false; !Once; Once = true, IsDone = DidBreak) \
    for ( \
        /* The user's loop variable: */ \
        const VarType VarName = Counter; \
        /* Set `DidBreak` to true at the start of the loop: */ \
        !Once && (DidBreak = true); \
        /* If loop exits normally, set `DidBreak` to false */ \
        DidBreak = false, Once = true)
// clang-format on
