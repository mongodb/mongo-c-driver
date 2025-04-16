#ifndef MLIB_DURATION_H_INCLUDED
#define MLIB_DURATION_H_INCLUDED

#include <mlib/ckdint.h>
#include <mlib/cmp.h>
#include <mlib/config.h>
#include <mlib/intutil.h>

#include <stdint.h>
#include <time.h>

mlib_extern_c_begin ();

/**
 * @brief The integral type used to represent a count of units of time.
 */
typedef int64_t mlib_duration_rep_t;

/**
 * @brief Represents a duration of time, either positive, negative, or zero.
 *
 * @note A zero-initialized (static initialized) duration represents the zero
 * duration (no elapsed time)
 *
 * @note The time representation is intended to be abstract, and should be
 * converted to concrete units of time by calling the `_count` functions.
 */
typedef struct mlib_duration {
   /**
    * @brief The integral representation of the duration.
    *
    * Do not read or modify this field except to zero-initialize it.
    */
   mlib_duration_rep_t _rep;
} mlib_duration;

/**
 * @brief A macro that expands to an `mlib_duration` representing no elapsed
 * time
 */
#define mlib_duration_zero() mlib_init (mlib_duration){0}
/**
 * @brief A macro that expands to the maximum positive duration
 */
#define mlib_duration_max()            \
   mlib_init (mlib_duration)           \
   {                                   \
      mlib_maxof (mlib_duration_rep_t) \
   }
/**
 * @brief A macro that expands to the minimum duration (a negative duration)
 */
#define mlib_duration_min()            \
   mlib_init (mlib_duration)           \
   {                                   \
      mlib_minof (mlib_duration_rep_t) \
   }

/**
 * @brief Obtain the count of microseconds represented by the duration (round
 * toward zero)
 */
static inline mlib_duration_rep_t
mlib_microseconds_count (const mlib_duration dur) mlib_noexcept
{
   return dur._rep;
}

/**
 * @brief Obtain the count of milliseconds represented by the duration (round
 * toward zero)
 */
static inline mlib_duration_rep_t
mlib_milliseconds_count (const mlib_duration dur) mlib_noexcept
{
   return mlib_microseconds_count (dur) / 1000;
}

/**
 * @brief Obtain the count of seconds represented by the duration (rounded
 * toward zero)
 */
static inline mlib_duration_rep_t
mlib_seconds_count (const mlib_duration dur) mlib_noexcept
{
   return mlib_milliseconds_count (dur) / 1000;
}

/**
 * @brief Create a duration object that represents the given number of
 * nanoseconds
 */
static inline mlib_duration
mlib_nanoseconds (const mlib_duration_rep_t n) mlib_noexcept
{
   // We encode as a count of microseconds, so we lose precision here.
   mlib_duration ret;
   ret._rep = n / 1000;
   return ret;
}

/**
 * @brief Create a duration object that represents the given number of
 * microseconds
 */
static inline mlib_duration
mlib_microseconds (const mlib_duration_rep_t n) mlib_noexcept
{
   mlib_duration ret;
   ret._rep = n;
   return ret;
}

/**
 * @brief Create a duration object that represents the given number of
 * milliseconds
 */
static inline mlib_duration
mlib_milliseconds (const mlib_duration_rep_t n) mlib_noexcept
{
   mlib_duration_rep_t clamp = 0;
   if (mlib_mul (&clamp, n, 1000)) {
      clamp = n > 0 ? mlib_maxof (mlib_duration_rep_t) : mlib_minof (mlib_duration_rep_t);
   }
   return mlib_microseconds (clamp);
}

/**
 * @brief Create a duration object that represents the given number of seconds
 */
static inline mlib_duration
mlib_seconds (const mlib_duration_rep_t n) mlib_noexcept
{
   mlib_duration_rep_t clamp = 0;
   if (mlib_mul (&clamp, n, 1000 * 1000)) {
      clamp = n > 0 ? mlib_maxof (mlib_duration_rep_t) : mlib_minof (mlib_duration_rep_t);
   }
   return mlib_microseconds (clamp);
}

/**
 * @brief Create a new duration that represents the sum of two other durations
 */
static inline mlib_duration
mlib_duration_add (const mlib_duration a, const mlib_duration b) mlib_noexcept
{
   mlib_duration ret = {0};
   if (mlib_add (&ret._rep, a._rep, b._rep)) {
      if (a._rep > 0) {
         ret = mlib_duration_max ();
      } else {
         ret = mlib_duration_min ();
      }
   }
   return ret;
}

/**
 * @brief Create a duration from subtracting the right-hand duration from
 * the left-hand duration (computes their difference)
 */
static inline mlib_duration
mlib_duration_sub (const mlib_duration a, const mlib_duration b) mlib_noexcept
{
   mlib_duration ret = {0};
   if (mlib_sub (&ret._rep, a._rep, b._rep)) {
      if (a._rep < 0) {
         ret = mlib_duration_min ();
      } else {
         ret = mlib_duration_max ();
      }
   }
   return ret;
}

/**
 * @brief Multiply a duration by some factor
 */
static inline mlib_duration
mlib_duration_mul (const mlib_duration dur, int fac) mlib_noexcept
{
   mlib_duration ret = {0};
   if (mlib_mul (&ret._rep, dur._rep, fac)) {
      if ((dur._rep < 0) != (fac < 0)) {
         // Different signs:  Neg × Pos = Neg
         ret = mlib_duration_min ();
      } else {
         // Same signs: Pos × Pos = Pos
         //             Neg × Neg = Pos
         ret = mlib_duration_max ();
      }
   }
   return ret;
}

/**
 * @brief Divide a duration by some divisor
 */
static inline mlib_duration
mlib_duration_div (mlib_duration a, int div) mlib_noexcept
{
   mlib_check (div, neq, 0);
   if (div == -1 && a._rep == mlib_minof (mlib_duration_rep_t)) {
      // MIN / -1 is UB, but the saturating result is the max
      a = mlib_duration_max ();
   } else {
      a._rep /= div;
   }
   return a;
}

/**
 * @brief Compare two durations
 *
 * @retval <0 If `a` is less-than `b`
 * @retval >0 If `b` is less-than `a`
 * @retval  0 If `a` and `b` are equal durations
 */
static inline enum mlib_cmp_result
mlib_duration_cmp (const mlib_duration a, const mlib_duration b) mlib_noexcept
{
   return mlib_cmp (a._rep, b._rep);
}

#define mlib_duration_cmp(...) MLIB_ARGC_PICK (_mlibDurationCmp, __VA_ARGS__)
#define _mlibDurationCmp_argc_2 mlib_duration_cmp
#define _mlibDurationCmp_argc_3(Left, Op, Right) (mlib_duration_cmp ((Left), (Right)) Op 0)

/**
 * @brief Obtain an mlib_duration that corresponds to a `timespec` value
 *
 * @note The `timespec` type may represent times outside of the range of, or
 * more precise than, what is representable in `mlib_duration`. In such case,
 * the returned duration will be the nearest representable duration, rounded
 * toward zero.
 */
static inline mlib_duration
mlib_duration_from_timespec (const struct timespec ts) mlib_noexcept
{
   return mlib_duration_add (mlib_seconds (ts.tv_sec), mlib_nanoseconds (ts.tv_nsec));
}

/**
 * @brief Create a C `struct timespec` that corresponds to the given duration
 *
 * @param d The duration to be converted
 * @return struct timespec A timespec that represents the same durations
 */
static inline struct timespec
mlib_duration_to_timespec (const mlib_duration d) mlib_noexcept
{
   // Number of full seconds to wait
   const mlib_duration_rep_t n_full_seconds = mlib_seconds_count (d);
   // Duration with full seconds removed:
   const mlib_duration usec_part = mlib_duration_sub (d, mlib_seconds (n_full_seconds));
   // Number of microseconds in the duration, minus all full seconds
   const mlib_duration_rep_t n_remaining_microseconds = mlib_microseconds_count (usec_part);
   // Compute the number of nanoseconds:
   const int32_t n_nsec = mlib_assert_mul (int32_t, n_remaining_microseconds, 1000);
   struct timespec ret;
   ret.tv_sec = n_full_seconds;
   ret.tv_nsec = n_nsec;
   return ret;
}

mlib_extern_c_end ();

#endif // MLIB_DURATION_H_INCLUDED
