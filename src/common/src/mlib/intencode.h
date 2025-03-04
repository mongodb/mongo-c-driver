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
