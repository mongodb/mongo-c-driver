/**
 * @file mlib/test.h
 * @brief Testing utilities
 * @date 2025-01-30
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

#include <mlib/config.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Place this macro at the head of a (compound) statement to assert that
 * executing that statement aborts the program with SIGABRT.
 *
 * Internally, this will fork the calling process and wait for the child process
 * to terminate. It asserts that the child exits abnormally with SIGABRT. This
 * test assertion is a no-op on Win32, since it does not have a suitable `fork`
 * API.
 *
 * Beware that the child process runs in a forked environment, so it is not
 * safe to use any non-fork-safe functionality, and any modifications to program
 * state will not be visible in the parent. Behavior of attempting to escape the
 * statement (goto/return) is undefined.
 *
 * If the child process does not abort, it will call `_Exit(71)` to indicate
 * to the parent that it did not terminate (the number 71 is chosen arbitrarily)
 *
 * If the token `debug` is passed as a macro argument, then the forking behavior
 * is suppressed, allowing for easier debugging of the statement.
 */
#define mlib_assert_aborts(...) MLIB_PASTE_3 (_mlibAssertAbortsStmt, _, __VA_ARGS__) ()

#ifndef _WIN32
#include <sys/wait.h>
#define _mlibAssertAbortsStmt_()                                                                  \
   for (int once = 1, other_pid = fork (); once; once = 0)                                        \
      for (; once; once = 0)                                                                      \
         if (other_pid != 0) {                                                                    \
            /* We are the parent */                                                               \
            int wstatus;                                                                          \
            waitpid (other_pid, &wstatus, 0);                                                     \
            if (WIFEXITED (wstatus)) {                                                            \
               /* Normal exit! */                                                                 \
               _mlib_stmt_did_not_abort (__FILE__, MLIB_FUNC, __LINE__, WEXITSTATUS (wstatus));   \
            } else if (WIFSIGNALED (wstatus)) {                                                   \
               /* Signalled */                                                                    \
               if (WTERMSIG (wstatus) != SIGABRT) {                                               \
                  fprintf (stderr,                                                                \
                           "%s:%d: [%s]: Child process did not exit with SIGABRT! (Exited %d)\n", \
                           __FILE__,                                                              \
                           __LINE__,                                                              \
                           MLIB_FUNC,                                                             \
                           WTERMSIG (wstatus));                                                   \
                  fflush (stderr);                                                                \
                  abort ();                                                                       \
               }                                                                                  \
            }                                                                                     \
         } else /* We are the child */                                                            \
            for (;; _Exit (71))                                                                   \
               for (;; _Exit (71)) /* Double loop to prevent the block from `break`ing out */

#else
#define _mlibAssertAbortsStmt_() \
   if (1) {                      \
   } else
#endif

// Called when an assert-aborts statement does not terminate
static inline void
_mlib_stmt_did_not_abort (const char *file, const char *func, int line, int rc)
{
   /* Normal exit! */
   if (rc == 71) {
      fprintf (stderr, "%s:%d: [%s]: Test case did not abort. The statement completed normally.\n", file, line, func);
   } else {
      fprintf (stderr, "%s:%d: [%s]: Test case did not abort (Exited %d)\n", file, line, func, rc);
   }
   fflush (stderr);
   abort ();
}

#define _mlibAssertAbortsStmt_debug()                                    \
   for (;; _mlib_stmt_did_not_abort (__FILE__, MLIB_FUNC, __LINE__, -1)) \
      for (;; _mlib_stmt_did_not_abort (__FILE__, MLIB_FUNC, __LINE__, -1))
