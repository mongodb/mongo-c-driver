/**
 * @file mlib/intencode.h
 * @brief Integer encoding functions
 * @date 2025-01-31
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
#include <mlib/loop.h>

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Decode an unsigned 32-bit little-endian integer from a memory buffer
 */
static inline uint32_t
mlib_read_u32le (const void *buf)
{
   uint32_t ret = 0;
   if (mlib_is_little_endian ()) {
      // Optimize: The platform uses a LE encoding already
      memcpy (&ret, buf, sizeof ret);
   } else {
      // Portable decode of an LE integer
      const uint8_t *cptr = (const uint8_t *) buf;
      mlib_foreach_urange (i, sizeof ret) {
         ret <<= 8;
         ret |= cptr[(sizeof ret) - i - 1];
      }
   }
   return ret;
}

/**
 * @brief Decode an signed 32-bit little-endian integer from a memory buffer
 */
static inline int32_t
mlib_read_i32le (const void *buf)
{
   const uint32_t u = mlib_read_u32le (buf);
   int32_t r;
   memcpy (&r, &u, sizeof r);
   return r;
}

/**
 * @brief Decode an unsigned 64-bit little-endian integer from a memory buffer
 */
static inline uint64_t
mlib_read_u64le (const void *buf)
{
   uint64_t ret = 0;
   if (mlib_is_little_endian ()) {
      // Optimize: The platform uses a LE encoding already
      memcpy (&ret, buf, sizeof ret);
   } else {
      // Portable decode of an LE integer
      const uint8_t *cptr = (const uint8_t *) buf;
      mlib_foreach_urange (i, sizeof ret) {
         ret <<= 8;
         ret |= cptr[(sizeof ret) - i - 1];
      }
   }
   return ret;
}

/**
 * @brief Decode an signed 64-bit little-endian integer from a memory buffer
 */
static inline int64_t
mlib_read_i64le (const void *buf)
{
   const uint64_t u = mlib_read_u64le (buf);
   int64_t r;
   memcpy (&r, &u, sizeof r);
   return r;
}

/**
 * @brief Write an unsigned 32-bit little-endian integer into a destination
 *
 * @return void* The address after the written value
 */
static inline void *
mlib_write_u32le (void *out, const uint32_t value)
{
   uint8_t *o = (uint8_t *) out;
   if (mlib_is_little_endian ()) {
      memcpy (o, &value, sizeof value);
      return o + sizeof value;
   }
   mlib_foreach_urange (i, sizeof value) {
      *o++ = (value >> (8u * i)) & 0xffu;
   }
   return o;
}

/**
 * @brief Write a signed 32-bit little-endian integer into a destination
 *
 * @return void* The address after the written value
 */
static inline void *
mlib_write_i32le (void *out, int32_t value)
{
   return mlib_write_u32le (out, (uint32_t) value);
}

/**
 * @brief Write an unsigned 64-bit little-endian integer into a destination
 *
 * @return void* The address after the written value
 */
static inline void *
mlib_write_u64le (void *out, const uint64_t value)
{
   uint8_t *o = (uint8_t *) out;
   if (mlib_is_little_endian ()) {
      memcpy (o, &value, sizeof value);
      return o + sizeof value;
   }
   mlib_foreach_urange (i, sizeof value) {
      *o++ = (value >> (8u * i)) & 0xffu;
   }
   return o;
}

/**
 * @brief Write an signed 64-bit little-endian integer into a destination
 *
 * @return void* The address after the written value
 */
static inline void *
mlib_write_i64le (void *out, int64_t value)
{
   return mlib_write_u64le (out, (uint64_t) value);
}

/**
 * @brief Write a little-endian 64-bit floating point (double) to the given
 * memory location
 *
 * @return void* The address after the written value.
 */
static inline void *
mlib_write_f64le (void *out, double d)
{
   mlib_static_assert (sizeof (double) == sizeof (uint64_t));
   uint64_t bits;
   memcpy (&bits, &d, sizeof d);
   return mlib_write_u64le (out, bits);
}

/**
 * @brief Test if the given codepoint is an ASCII-range decimal digit
 *
 * @param i A 32-bit character codepoint, or converted code unit
 * @return true If-and-only-if the codepoint 'i' is a digit
 * @return false Otherwise
 */
static inline bool
mlib_isdigit (int32_t i)
{
   // U+0030 '0'
   // U+0039 '9'
   return i >= 0x30 && i <= 0x39;
}

/**
 * @brief Result of parsing an int64_t from a string
 */
typedef struct mlib_i64_parse_result {
   /**
    * @brief Upon success, this is the value that was parsed from the input string.
    *
    * If the parsing failed for any reason, then this is set to an unspecified
    * sentinel value that stands out when read in a debugger.
    */
   int64_t value;
   /**
    * @brief The error code for the parse. Comes from an `errno` value.
    *
    * - A vlaue of `0` indicates that the parse was successful
    * - A value of `EINVAL` indicates that the input string is not a valid
    *   representation of an integer.
    * - A value of `ERANGE` indicates thath the input string is a valid integer,
    *   but the actual encoded value cannot be represented in an `int64_t`
    */
   int ec;
} mlib_i64_parse_result;

/**
 * @brief Parse a C string as a 64-bit signed integer
 *
 * @param in Pointer to the beginning of the null-terminated string
 * @param base Optional: The base to use for parsing. Use "0" to infer the base.
 * @return mlib_i64_parse_result The result of parsing, including the value and
 * error information.
 *
 * This differs from `strtoll` in that it requires that the entire string be
 * parsed as a valid integer. If parsing stops early, then the result will indicate
 * an error of EINVAL.
 */
static inline mlib_i64_parse_result
mlib_i64_parse (const char *const in, int base)
{
   const size_t len = strlen (in);
   // THe 2424242... is a sentinel value for debugging and troubleshooting
   mlib_i64_parse_result ret = {2424242424242424242, 0};
   if (len == 0) {
      // Empty string is not a valid integer
      ret.ec = EINVAL;
      return ret;
   }
   // Check that the first char is a valid integer start. We don't want to use
   // strtoll's whitespace-skipping behavior
   if (!mlib_isdigit (in[0]) && in[0] != '+' && in[0] != '-') {
      ret.ec = EINVAL;
      return ret;
   }
   // Rely on stroll for our underlying parsing
   errno = 0;
   char *eptr;
   const int64_t val = strtoll (in, &eptr, base);
   ret.ec = errno;

   // Check that we parsed the full string.
   const char *end = in + strlen (in);
   if (end != eptr) {
      // Did not parse the full string as an integer. Why not?
      const char *scan = eptr;
      for (; scan != end; ++scan) {
         if (!mlib_isdigit (*scan)) {
            // Found a non-digit character. Replace the error with EINVAL, to indicate
            // that the input string is not a valid integer spelling
            ret.ec = EINVAL;
            // Note: strtoll might have set ERANGE if the prefix was also
            // too large, but we prioritize EINVAL to indicate that the string
            // is not a valid integer, even if we had infinite precision
         }
      }
   }
   // Only return a non-zero value if we didn't get an error
   if (!ret.ec) {
      ret.value = val;
   }
   return ret;
}

#define mlib_i64_parse(...) MLIB_ARGC_PICK (_mlib_i64_parse, __VA_ARGS__)
#define _mlib_i64_parse_argc_2(S, Base) mlib_i64_parse (S, Base)
#define _mlib_i64_parse_argc_1(S) _mlib_i64_parse_argc_2 (S, 0)
