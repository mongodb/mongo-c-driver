/**
 * @file mlib/time_point.h
 * @brief A point-in-time type
 * @date 2025-04-17
 *
 * The `mlib_time_point` type represents a stable point-in-time. The time point
 * itself is relative to a monotonic clock for the program, so it should not be
 * transmitted or persisted outside of the execution of a program that uses it.
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
#ifndef MLIB_TIME_POINT_H_INCLUDED
#define MLIB_TIME_POINT_H_INCLUDED

#include <mlib/cmp.h>
#include <mlib/config.h>
#include <mlib/duration.h>
#include <mlib/platform.h>

// Check for POSIX clock functions functions
#if (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L) || (defined(_DEFAULT_SOURCE) && !defined(_WIN32))
#include <sys/types.h>
#define mlib_have_posix_clocks() 1
#endif

#ifndef mlib_have_posix_clocks
#define mlib_have_posix_clocks() 0
#endif

#include <errno.h>
#include <time.h>

mlib_extern_c_begin ();

/**
 * @brief An abstract point-in-time type
 *
 * This type represents an abstract stable point in time.
 */
typedef struct mlib_time_point {
   /**
    * @brief The encoding of the time point as a duration relative to some
    * unspecified stable real point in time.
    *
    * The stable time point may change between program executions, so this
    * object should not be stored outside of the program's execution.
    */
   mlib_duration _time_since_monotonic_start;
} mlib_time_point;

/**
 * @brief Obtain a point-in-time corresponding to the current time
 */
static inline mlib_time_point
mlib_now (void) mlib_noexcept
{
#if mlib_have_posix_clocks()
   // Use POSIX clock_gettime
   struct timespec ts;
   // Use the POSIX monotonic clock
   int rc = clock_gettime (CLOCK_MONOTONIC, &ts);
   // The above call must never fail:
   mlib_check (rc, eq, 0);
   // Encode the time point:
   mlib_time_point ret;
   ret._time_since_monotonic_start = mlib_duration_from_timespec (ts);
   return ret;
#elif defined(_WIN32)
   // Win32 APIs for the high-performance monotonic counter. These APIs never fail after Windows XP
   LARGE_INTEGER freq;
   QueryPerformanceFrequency (&freq);
   LARGE_INTEGER lits;
   QueryPerformanceCounter (&lits);
   // Number of ticks of the perf counter
   const int64_t ticks = lits.QuadPart;
   // Number of ticks that the counter emits in one second
   const int64_t ticks_per_second = freq.QuadPart;
   // Do some math that avoids an integer overflow when converting to microseconds.
   // Just one million, used to convert time units to microseconds.
   const int64_t one_million = 1000000;
   // Number of whole seconds that have elapsed:
   const int64_t whole_seconds = ticks / ticks_per_second;
   // Times one million:
   const int64_t whole_seconds_1m = whole_seconds * one_million;
   // Number of microseconds beyond the last whole second:
   const int64_t subsecond_us = ((ticks % ticks_per_second) * one_million) / ticks_per_second;
   mlib_time_point ret;
   ret._time_since_monotonic_start = mlib_duration ((whole_seconds_1m, us), plus, (subsecond_us, us));
   return ret;
#else
#error We do not know how to get the current time on this platform
#endif
}

/**
 * @brief Obtain a point-in-time relative to a base time offset by the given
 * duration (which may be negative).
 *
 * @param from The basis of the time offset
 * @param delta The amount of time to shift the resulting time point
 * @return mlib_time_point If 'delta' is a positive duration, the result is a
 * point-in-time *after* 'from'. If 'delta' is a negative duration, the result
 * is a point-in-time *before* 'from'.
 */
static inline mlib_time_point
mlib_later (mlib_time_point from, mlib_duration delta) mlib_noexcept
{
   mlib_time_point ret;
   ret._time_since_monotonic_start = mlib_duration (from._time_since_monotonic_start, plus, delta);
   return ret;
}

#define mlib_later(From, Delta) mlib_later (From, MLIB_EVAL_4 (_mlibDurationArgument (Delta)))

/**
 * @brief Obtain the duration between two points in time.
 *
 * @param then The target time
 * @param from The base time
 * @return mlib_duration The amount of time you would need to wait starting
 * at 'from' for the time to become 'then' (the result may be a negative
 * duration).
 *
 * Intuition: If "then" is "in the future" relative to "from", you will
 * receive a positive duration, indicating an amount of time to wait
 * beginning at 'from' to reach 'then'. If "then" is actually *before*
 * "from", you will receive a paradoxical *negative* duration, indicating
 * the amount of time needed to time-travel backwards to reach "then."
 */
static inline mlib_duration
mlib_time_difference (mlib_time_point then, mlib_time_point from)
{
   return mlib_duration (then._time_since_monotonic_start, minus, from._time_since_monotonic_start);
}

/**
 * @brief Compare two time points to create an ordering.
 *
 * A time point "in the past" is "less than" a time point "in the future".
 *
 * @retval <0 If 'a' is before 'b'
 * @retval >0 If 'b' is before 'a'
 * @retval  0 If 'a' and 'b' are equivalent
 *
 * @note This is a function-like macro that can be called with an infix operator
 * as the second argument to do natural time-point comparisons:
 *
 * ```
 *    mlib_time_cmp(a, <=, b)
 * ```
 */
static inline enum mlib_cmp_result
mlib_time_cmp (mlib_time_point a, mlib_time_point b) mlib_noexcept
{
   return mlib_duration_cmp (a._time_since_monotonic_start, b._time_since_monotonic_start);
}

#define mlib_time_cmp(...) MLIB_ARGC_PICK (_mlib_time_cmp, __VA_ARGS__)
#define _mlib_time_cmp_argc_2 mlib_time_cmp
#define _mlib_time_cmp_argc_3(L, Op, R) (mlib_time_cmp ((L), (R)) Op 0)

/**
 * @brief Pause the calling thread until at least the specified duration has elapsed.
 *
 * @param d The duration of time to pause the thread. If this duration is zero
 * or negative, then this function returns immediately.
 * @return int An error code, if any occurred. Returns zero upon success, or
 * the system's error number value (`errno` on POSIX, `GetLastError()` on
 * Windows)
 */
static inline int
mlib_sleep_for (const mlib_duration d) mlib_noexcept
{
   mlib_duration_rep_t duration_usec = mlib_microseconds_count (d);
   if (duration_usec <= 0) {
      // Don't sleep any time
      return 0;
   }
#if mlib_have_posix_clocks()
   // Convert the microseconds count to the value for the usleep function
   useconds_t i = 0;
   if (mlib_narrow (&i, mlib_microseconds_count (d))) {
      // Too many microseconds. Sleep for the max.
      i = mlib_maxof (useconds_t);
   }
   int rc = usleep (mlib_microseconds_count (d));
   if (rc != 0) {
      return errno;
   }
   return 0;
#elif defined(_WIN32)
   DWORD retc = 0;
   // Use WaitableTimer
   const HANDLE timer = CreateWaitableTimerW (/* no attributes */ NULL,
                                              /* Manual reset */ true,
                                              /* Unnamed */ NULL);
   // Check that we actually succeeded in creating a timer.
   if (!timer) {
      retc = GetLastError ();
      goto done;
   }
   // Convert the number of microseconds into a count of 100ns intervals. Use
   // a negative value to request a relative sleep time.
   LONGLONG negative_n_100ns_units = 0;
   if (mlib_mul (&negative_n_100ns_units, duration_usec, -10)) {
      // Too many units. Clamp to the max duration (negative for a relative
      // sleep):
      negative_n_100ns_units = mlib_minof (LONGLONG);
   }
   LARGE_INTEGER due_time;
   due_time.QuadPart = negative_n_100ns_units;
   BOOL okay = SetWaitableTimer (/* The timer to modify */ timer,
                                 /* The time after which it will fire */ &due_time,
                                 /* Interval period 0 = only fire once */ 0,
                                 /* No completion routine */ NULL,
                                 /* No arg for no completion routine */ NULL,
                                 /* Wake up the system if it goes to sleep */ true);
   if (!okay) {
      // Failed to set the timer. Hmm?
      retc = GetLastError ();
      goto done;
   }
   // Do the actual wait
   DWORD rc = WaitForSingleObject (timer, INFINITE);
   if (rc == WAIT_FAILED) {
      // Executing the wait operation failed.
      retc = GetLastError ();
      goto done;
   }
   // Check for success:
   mlib_check (rc, eq, WAIT_OBJECT_0);
done:
   // Done with the timer.
   if (timer) {
      CloseHandle (timer);
   }
   return retc;
#else
#error "mlib_sleep_for" is not implemented on this platform.
#endif
}

#define mlib_sleep_for(...) mlib_sleep_for (mlib_duration (__VA_ARGS__))

/**
 * @brief Pause the calling thread until the given time point has been reached
 *
 * @param when The time point at which to resume, at soonest
 * @return int A possible error code for the operation. Returns zero upon success.
 *
 * The `when` is the *soonest* successful wake time. The thread may wake at a later time.
 */
static inline int
mlib_sleep_until (const mlib_time_point when) mlib_noexcept
{
   const mlib_duration time_until = mlib_time_difference (when, mlib_now ());
   return mlib_sleep_for (time_until);
}

mlib_extern_c_end ();

#endif // MLIB_TIME_POINT_H_INCLUDED
