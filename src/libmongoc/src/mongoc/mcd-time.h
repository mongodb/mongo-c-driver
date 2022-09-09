/**
 * Copyright 2022 MongoDB, Inc.
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

#ifndef MCD_TIME_H_INCLUDED
#define MCD_TIME_H_INCLUDED

#include "mongoc-prelude.h"

#include "./mcd-integer.h"

#include <bson/bson.h>


/**
 * @brief Represents an abstract point-in-time.
 *
 * @note This is an *abstract* time point, with the only guarantee that it
 * is strictly ordered with every other time point and that the difference
 * between any two times will roughly encode actual wall-clock durations.
 */
typedef struct mcd_time_point {
   /// The internal representation of the time.
   int64_t _rep;
} mcd_time_point;

/**
 * @brief Represents a (possibly negative) duration of time.
 *
 * Construct this using one of the duration constructor functions.
 *
 * @note This encodes real wall-time durations, and may include negative
 * durations. It can be compared with other durations and used to offset
 * time points.
 */
typedef struct mcd_duration {
   /// An internal representation of the duration
   int64_t _rep;
} mcd_duration;

/**
 * @brief Obtain the current time point. This is only an abstract
 * monotonically increasing time, and does not necessarily correlate with
 * any real-world clock.
 */
static inline mcd_time_point
mcd_now (void)
{
   // Create a time point representing the current time.
   return (mcd_time_point){._rep = bson_get_monotonic_time ()};
}

/// Create a duration from a number of microseconds
static inline mcd_duration
mcd_microseconds (int64_t s)
{
   // 'mcd_duration' is encoded in a number of microseconds
   return (mcd_duration){._rep = s};
}

/// Create a duration from a number of milliseconds
static inline mcd_duration
mcd_milliseconds (int64_t s)
{
   // 1'000 microseconds per millisecond:
   BSON_ASSERT (!_mcd_i64_mul_would_overflow (s, 1000));
   return mcd_microseconds (s * 1000);
}

/// Create a duration from a number of seconds
static inline mcd_duration
mcd_seconds (int64_t s)
{
   // 1'000 milliseconds per second:
   BSON_ASSERT (!_mcd_i64_mul_would_overflow (s, 1000));
   return mcd_milliseconds (s * 1000);
}

/// Create a duration from a number of minutes
static inline mcd_duration
mcd_minutes (int64_t m)
{
   // Sixty seconds per minute:
   BSON_ASSERT (!_mcd_i64_mul_would_overflow (m, 60));
   return mcd_seconds (m * 60);
}

/// Convert an abstract duration to a number of milliseconds
static inline int64_t
mcd_get_milliseconds (mcd_duration d)
{
   return d._rep / 1000;
}

/// Obtain the time point relative to a base time as if by waiting for
/// `delta` amount of time (which may be negatve)
static inline mcd_time_point
mcd_later (mcd_time_point from, mcd_duration delta)
{
   BSON_ASSERT (!_mcd_i64_add_would_overflow (from._rep, delta._rep));
   from._rep += delta._rep;
   return from;
}

/**
 * @brief Obtain the duration between two points in time.
 *
 * @param then The target time
 * @param from The base time
 * @return mcd_duration The amount of time you would need to wait starting
 * at 'from' for the time to become 'then' (the result may be a negative
 * duration).
 *
 * Intuition: If "then" is "in the future" relative to "from", you will
 * receive a positive duration, indicating an amount of time to wait
 * beginning at 'from' to reach 'then'. If "then" is actually *before*
 * "from", you will receive a paradoxical *negative* duration, indicating
 * the amount of time needed to time-travel backwards to reach "then."
 */
static inline mcd_duration
mcd_time_difference (mcd_time_point then, mcd_time_point from)
{
   BSON_ASSERT (!_mcd_i64_sub_would_overflow (then._rep, from._rep));
   int64_t diff = then._rep - from._rep;
   // Our time_point encodes the time using a microsecond counter.
   return mcd_microseconds (diff);
}

/**
 * @brief Compare two time points to create an ordering.
 *
 * A time point "in the past" is "less than" a time point "in the future".
 *
 * @retval <0 If 'left' is before 'right'
 * @retval >0 If 'right' is before 'left'
 * @retval  0 If 'left' and 'right' are equivalent
 */
static inline int
mcd_time_compare (mcd_time_point left, mcd_time_point right)
{
   // Obtain the amount of time needed to wait from 'right' to reach
   // 'left'
   int64_t diff = mcd_time_difference (left, right)._rep;
   if (diff < 0) {
      // A negative duration indicates that 'left' is "before" 'right'
      return -1;
   } else if (diff > 0) {
      // A positive duration indicates that 'left' is "after" 'right'
      return 1;
   } else {
      // These time points are equivalent
      return 0;
   }
}

/**
 * @brief Compare two durations
 *
 * A duration D1 is "less than" a duration D2 if time-travelling/waiting for D1
 * duration would end in the past relative to time-travelling/waiting for D2.
 *
 * @retval <0 If left is "less than" right
 * @retval >0 If left is "greater than" right
 * @retval  0 If left and right are equivalent
 */
static inline int
mcd_duration_compare (mcd_duration left, mcd_duration right)
{
   if (left._rep < right._rep) {
      return -1;
   } else if (left._rep > right._rep) {
      return 1;
   } else {
      return 0;
   }
}

/// Represents a timer that can be expired
typedef struct mcd_timer {
   /// The point in time after which the time will become expired.
   mcd_time_point expire_at;
} mcd_timer;

/// Create a time that will expire at the given time
static inline mcd_timer
mcd_timer_expire_at (mcd_time_point time)
{
   return (mcd_timer){time};
}

/**
 * @brief Create a timer that will expire after waiting for the given duration
 * relative to now
 *
 * @note If the duration is less-than zero, the timer will already have expired
 */
static inline mcd_timer
mcd_timer_expire_after (mcd_duration after)
{
   return mcd_timer_expire_at (mcd_later (mcd_now (), after));
}

/**
 * @brief Obtain the amount of time that one will need to WAIT before the timer
 * will be an expired state.
 *
 * @return mcd_duration A non-negative duration.
 *
 * @note If the timer is already expired, returns a zero duration. Will never
 * return a negative duration.
 */
static inline mcd_duration
mcd_timer_remaining (mcd_timer timer)
{
   // Compute the distance until the expiry time relative to now
   mcd_duration remain = mcd_time_difference (timer.expire_at, mcd_now ());
   // Compare that duration with a zero duration
   if (mcd_duration_compare (remain, mcd_microseconds (0)) < 0) {
      // The "remaining" time is less-than zero, which means the timer is
      // already expired, so we only need to wait for zero time:
      return mcd_microseconds (0);
   }
   // There is a positive amount of time remaining
   return remain;
}

#endif // MCD_TIME_H_INCLUDED
