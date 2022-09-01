#ifndef MCD_TIME_H_INCLUDED
#define MCD_TIME_H_INCLUDED

#include <mongoc-prelude.h>

#include <inttypes.h>

#include <bson/bson.h>

/**
 * @brief Represents an abstract point-in-time.
 *
 * @note This is an *abstract* time point, with the only guarantee that it is
 * strictly ordered with every other time point and that the difference between
 * any two times will roughly encode actual wall-clock durations.
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
 * durations. It can be compared with other durations and used to offset time
 * points.
 */
typedef struct mcd_duration {
   /// An internal representation of the duration
   int64_t _rep;
} mcd_duration;

/**
 * @brief Obtain the current time point. This is only an abstract monotonically
 * increasing time, and does not necessarily correlate with any real-world
 * clock.
 */
static inline struct mcd_time_point
mcd_now (void)
{
   // Create a time point representing the current time.
   return (mcd_time_point){._rep = bson_get_monotonic_time ()};
}

/// Create a duration from a number of microseconds
inline static mcd_duration
mcd_microseconds (int64_t s)
{
   // 'mcd_duration' is encoded in a number of microseconds
   return (mcd_duration){._rep = s};
}

/// Create a duration from a number of milliseconds
inline static mcd_duration
mcd_milliseconds (int64_t s)
{
   // 1'000 microseconds per millisecond:
   return mcd_microseconds (s * 1000);
}

/// Create a duration from a number of seconds
inline static mcd_duration
mcd_seconds (int64_t s)
{
   // 1'000 milliseconds per second:
   return mcd_milliseconds (s * 1000);
}

/// Create a duration from a number of minutes
inline static mcd_duration
mcd_minutes (int64_t m)
{
   // Sixty seconds per minute:
   return mcd_seconds (m * 60);
}

/// Obtain the time point relative to a base time as if by waiting for `delta`
/// amount of time (which may be negatve)
inline static mcd_time_point
mcd_later (mcd_time_point from, mcd_duration delta)
{
   from._rep += delta._rep;
   return from;
}

/**
 * @brief Obtain the duration between two points in time.
 *
 * @param then The target time
 * @param from The base time
 * @return mcd_duration The amount of time you would need to wait starting at
 * 'from' for the time to become 'then' (the result may be a negative
 * duration).
 *
 * Intuition: If "then" is "in the future" relative to "from", you will
 * receive a positive duration, indicating an amount of time to wait beginning
 * at 'from' to reach 'then'. If "then" is actually *before* "from", you will
 * receive a paradoxical *negative* duration, indicating the amount of time
 * needed to time-travel backwards to reach "then."
 */
inline static mcd_duration
mcd_time_difference (mcd_time_point then, mcd_time_point from)
{
   int64_t diff = then._rep - from._rep;
   // Our time_point encodes the time using a microsecond counter.
   return mcd_microseconds (diff);
}

/**
 * @brief Compare two time points to create an ordering.
 *
 * A time point "in the past" is "less than" a time point "in the future".
 *
 * @retval -1 If 'left' is before 'right'
 * @retval +1 If 'right' is before 'left'
 * @retval  0 If 'left' and 'right' are equivalent
 */
inline static int
mcd_time_compare (mcd_time_point left, mcd_time_point right)
{
   // Obtain the amount of time needed to wait from 'right' to reach 'left'
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

#endif // MCD_TIME_H_INCLUDED
