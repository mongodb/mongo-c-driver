/*
 * Copyright 2009-present MongoDB, Inc.
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

#include "common-prelude.h"

#ifndef MONGO_C_DRIVER_COMMON_STRING_PRIVATE_H
#define MONGO_C_DRIVER_COMMON_STRING_PRIVATE_H

#include <bson/bson.h>
#include <string.h>
#include "common-cmp-private.h"


/* Until the deprecated bson_string_t is removed, this must have the same members in the same order, so we can safely
 * cast between the two types. Afterward, we are free to modify the memory layout as needed.
 *
 * In mcommon_string_t, 'str' is guaranteed to be NUL terminated and SHOULD be valid UTF-8. mcommon_string_t operations
 * MUST maintain the validity of valid UTF-8 strings.
 *
 * Unused portions of the buffer may be uninitialized, and must not be compared or copied.
 *
 * 'len' is measured in bytes, not including the NUL terminator.
 *
 * 'alloc' is the actual length of the bson_malloc() allocation in bytes, including the required space for NUL
 * termination.
 *
 * When we use 'capacity', it refers to the largest 'len' that the buffer could store. alloc == capacity + 1.
 */
typedef struct mcommon_string_t {
   char *str;
   uint32_t len;
   uint32_t alloc;
} mcommon_string_t;

/* Parameters and outcome for a bounded append operation on a mcommon_string_t. Individual type-specific append
 * functions can consume this struct to communicate bounds info. "max_len_exceeded" can be tested any time an
 * algorithmic exit is convenient; the actual appended content will be limited by max_len. Truncation is guaranteed not
 * to split a valid UTF-8 byte sequence.
 *
 * Members are here to support inline definitions; not intended for direct access.
 *
 * Multiple mcommon_string_append_t may simultaneously refer to the same 'string' but this usage is not recommended.
 *
 * 'max_len_exceeded' only includes operations undertaken on this specific mcommon_string_append_t. It will not be set
 * if the string was already overlong, or if a different mcommon_string_append_t experiences an overage.
 */
typedef struct mcommon_string_append_t {
   mcommon_string_t *_string;
   uint32_t _max_len;
   bool _max_len_exceeded;
} mcommon_string_append_t;

#define mcommon_string_new_with_capacity COMMON_NAME (string_new_with_capacity)
#define mcommon_string_new_with_buffer COMMON_NAME (string_new_with_buffer)
#define mcommon_string_destroy COMMON_NAME (string_destroy)
#define mcommon_string_destroy_into_buffer COMMON_NAME (string_destroy_into_buffer)
#define mcommon_string_grow_to_capacity COMMON_NAME (string_grow_to_capacity)
#define mcommon_string_append_selected_chars COMMON_NAME (string_append_selected_chars)
#define mcommon_string_append_bytes_internal COMMON_NAME (string_append_bytes_internal)
#define mcommon_string_append_bytes_atomic COMMON_NAME (string_append_bytes_atomic)
#define mcommon_string_append_unichar_internal COMMON_NAME (string_append_unichar_internal)
#define mcommon_string_append_base64_encode COMMON_NAME (string_append_base64_encode)
#define mcommon_string_append_oid_as_hex COMMON_NAME (string_append_oid_as_hex)
#define mcommon_string_append_printf COMMON_NAME (string_append_printf)
#define mcommon_string_append_vprintf COMMON_NAME (string_append_vprintf)

bool
mcommon_string_append_bytes_internal (mcommon_string_append_t *append, const char *str, uint32_t len);

bool
mcommon_string_append_unichar_internal (mcommon_string_append_t *append, bson_unichar_t unichar);

/**
 * @brief Allocate a new mcommon_string_t with a copy of the supplied initializer string and an explicit buffer
 * capacity.
 *
 * @param str Initializer string, should be valid UTF-8.
 * @param length Length of initializer string, in bytes.
 * @param min_capacity Minimum string capacity, in bytes, the buffer must be able to store without reallocating. Does
 * not include the NUL terminator. Must be less than UINT32_MAX.
 * @returns A new mcommon_string_t that must be freed with mcommon_string_destroy() or
 * mcommon_string_destroy_into_buffer() and bson_free(). It will hold 'str' in its entirety, even if the requested
 * min_capacity was smaller.
 */
mcommon_string_t *
mcommon_string_new_with_capacity (const char *str, uint32_t length, uint32_t min_capacity);

/**
 * @brief Allocate a new mcommon_string_t with a copy of the supplied initializer string and a minimum-capacity buffer
 *
 * @param str NUL terminated string, should be valid UTF-8. Must be less than UINT32_MAX bytes long, overlong input
 * causes a runtime assertion failure.
 * @returns A new mcommon_string_t that must be freed with mcommon_string_destroy() or
 * mcommon_string_destroy_into_buffer() and bson_free().
 */
static BSON_INLINE mcommon_string_t *
mcommon_string_new (const char *str)
{
   BSON_ASSERT_PARAM (str);
   size_t length = strlen (str);
   BSON_ASSERT (mcommon_in_range_unsigned (uint32_t, length) && (uint32_t) length < UINT32_MAX);
   return mcommon_string_new_with_capacity (str, (uint32_t) length, 0);
}

/**
 * @brief Allocate a new mcommon_string_t, taking ownership of an existing buffer
 *
 * @param buffer Buffer to adopt, suitable for bson_free() and bson_realloc().
 * @param length Length of the string data, in bytes, not including the required NUL terminator. If string data is
 * present, it should be valid UTF-8.
 * @param alloc Actual allocated size of the buffer, in bytes, including room for NUL termination.
 * @returns A new mcommon_string_t that must be freed with mcommon_string_destroy() or
 * mcommon_string_destroy_into_buffer() and bson_free().
 */
mcommon_string_t *
mcommon_string_new_with_buffer (char *buffer, uint32_t length, uint32_t alloc);

/**
 * @brief Deallocate a mcommon_string_t and its internal buffer
 * @param string String allocated with mcommon_string_new, or NULL.
 */
void
mcommon_string_destroy (mcommon_string_t *string);

/**
 * @brief Deallocate a mcommon_string_t and return its internal buffer as a NUL-terminated C string.
 * @param string String allocated with mcommon_string_new, or NULL.
 * @returns A freestanding NUL-terminated string in a buffer that must be freed with bson_free(), or NULL if 'string'
 * was NULL.
 */
char *
mcommon_string_destroy_into_buffer (mcommon_string_t *string);

/**
 * @brief Truncate the string to zero length without deallocating the buffer
 * @param string String to clear
 */
static BSON_INLINE void
mcommon_string_clear (mcommon_string_t *string)
{
   string->len = 0;
   string->str[0] = '\0';
}

/**
 * @brief Test if the string has zero length
 * @param string String to test
 */
static BSON_INLINE bool
mcommon_string_is_empty (const mcommon_string_t *string)
{
   return string->len == 0;
}

/**
 * @brief Test if the string begins with a C string
 * @param string mcommon_string_t to test
 * @param substring prefix to match, as a NUL terminated C string.
 */
static BSON_INLINE bool
mcommon_string_starts_with_str (const mcommon_string_t *string, const char *substring)
{
   size_t substring_len = strlen (substring);
   uint32_t string_len = string->len;
   if (mcommon_in_range_unsigned (uint32_t, substring_len) && (uint32_t) substring_len <= string_len) {
      return 0 == memcmp (string->str, substring, substring_len);
   } else {
      return false;
   }
}

/**
 * @brief Test if the string ends with a C string
 * @param string mcommon_string_t to test
 * @param substring suffix to match, as a NUL terminated C string.
 */
static BSON_INLINE bool
mcommon_string_ends_with_str (const mcommon_string_t *string, const char *substring)
{
   size_t substring_len = strlen (substring);
   uint32_t string_len = string->len;
   if (mcommon_in_range_unsigned (uint32_t, substring_len) && (uint32_t) substring_len <= string_len) {
      uint32_t offset = string_len - (uint32_t) substring_len;
      return 0 == memcmp (string->str + offset, substring, substring_len);
   } else {
      return false;
   }
}

/**
 * @brief Grow a mcommon_string_t buffer if necessary to ensure a minimum capacity
 *
 * @param string String allocated with mcommon_string_new
 * @param capacity Minimum string length, in bytes, the buffer must be able to store without reallocating. Does not
 * include the NUL terminator. Must be less than UINT32_MAX.
 *
 * If a reallocation is necessary, the actual allocation size will be chosen as the next highest power-of-two above the
 * minimum needed to store 'capacity' as well as the NUL terminator.
 */
void
mcommon_string_grow_to_capacity (mcommon_string_t *string, uint32_t capacity);

/**
 * @brief Begin appending to an mcommon_string_t, with an explicit length limit
 * @param new_append Pointer to an uninitialized mcommon_string_append_t
 * @param string String allocated with mcommon_string_new
 * @param max_len Maximum allowed length for the resulting string, in bytes. Must be less than UINT32_MAX.
 *
 * The mcommon_string_append_t does not need to be deallocated. It is no longer usable if the underlying
 * mcommon_string_t is freed.
 *
 * If the string was already over maximum length, it will not be modified. All append operations are guaranteed not to
 * lengthen the string beyond max_len. Truncations are guaranteed to happen at UTF-8 code point boundaries.
 */
static BSON_INLINE void
mcommon_string_append_init_with_limit (mcommon_string_append_t *new_append, mcommon_string_t *string, uint32_t max_len)
{
   BSON_ASSERT_PARAM (new_append);
   BSON_ASSERT_PARAM (string);
   BSON_ASSERT (max_len < UINT32_MAX);
   new_append->_string = string;
   new_append->_max_len = max_len;
   new_append->_max_len_exceeded = false;
}

/**
 * @brief Begin appending to an mcommon_string_t
 * @param new_append Pointer to an uninitialized mcommon_string_append_t
 * @param string String allocated with mcommon_string_new
 *
 * The mcommon_string_append_t does not need to be deallocated. It is no longer usable if the underlying
 * mcommon_string_t is freed.
 *
 * The maximum string length will be set to the largest representable by the data type, UINT32_MAX - 1.
 */
static BSON_INLINE void
mcommon_string_append_init (mcommon_string_append_t *new_append, mcommon_string_t *string)
{
   mcommon_string_append_init_with_limit (new_append, string, UINT32_MAX - 1u);
}

/**
 * @brief Begin appending to a new empty mcommon_string_t with default initial capacity and maximum length
 * @param new_append Pointer to an uninitialized mcommon_string_append_t
 *
 * Allocates a new mcommon_string_t, which will need to be deallocated by the caller.
 * The mcommon_string_append_t itself does not need to be deallocated.
 *
 * The maximum string length will be set to the largest representable by the data type, UINT32_MAX - 1.
 * The new string will be allocated with a small default capacity.
 *
 * This method is intended to be the most convenient way to start growing a string. If a reasonable guess
 * can be made about the final size of the string, it's better to call mcommon_string_new_with_capacity()
 * and mcommon_string_append_init() directly.
 */
static BSON_INLINE void
mcommon_string_append_new (mcommon_string_append_t *new_append)
{
   mcommon_string_append_init (new_append, mcommon_string_new_with_capacity ("", 0, 32));
}

/**
 * @brief Begin appending to a new empty mcommon_string_t with a default capacity and a matching max append length.
 * @param new_append Pointer to an uninitialized mcommon_string_append_t
 *
 * Allocates a new mcommon_string_t, which will need to be deallocated by the caller.
 * The mcommon_string_append_t itself does not need to be deallocated.
 */
static BSON_INLINE void
mcommon_string_append_new_fixed_capacity (mcommon_string_append_t *new_append, uint32_t capacity)
{
   mcommon_string_append_init_with_limit (new_append, mcommon_string_new_with_capacity ("", 0, capacity), capacity);
}

/**
 * @brief Check the status of an append operation
 * @param append Append operation, initialized with mcommon_string_append_init
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 */
static BSON_INLINE bool
mcommon_string_append_status (const mcommon_string_append_t *append)
{
   return !append->_max_len_exceeded;
}

/**
 * @brief Get a reference to the string being appended to
 * @param append Append operation, initialized with mcommon_string_append_init
 * @returns Pointer to the mcommon_string_t being written to
 */
static BSON_INLINE mcommon_string_t *
mcommon_string_append_destination (const mcommon_string_append_t *append)
{
   return append->_string;
}

/**
 * @brief Deallocate the mcommon_string_t destination associated with an mcommon_string_append_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * The append operation will no longer be usable after this call.
 */
static BSON_INLINE void
mcommon_string_append_destination_destroy (const mcommon_string_append_t *append)
{
   mcommon_string_destroy (mcommon_string_append_destination (append));
}

/**
 * @brief Deallocate the mcommon_string_t destination associated with an mcommon_string_append_t and return its internal
 * buffer
 * @param append Append operation, initialized with mcommon_string_append_init
 * @returns A freestanding NUL-terminated string in a buffer that must be freed with bson_free()
 * The append operation will no longer be usable after this call.
 */
static BSON_INLINE char *
mcommon_string_append_destination_destroy_into_buffer (const mcommon_string_append_t *append)
{
   return mcommon_string_destroy_into_buffer (mcommon_string_append_destination (append));
}

/**
 * @brief Test if the append destination ends with a C string
 * @param string mcommon_string_append_t with the string to test
 * @param substring suffix to match, as a NUL terminated C string.
 */
static BSON_INLINE bool
mcommon_string_append_ends_with_str (const mcommon_string_append_t *append, const char *substring)
{
   return mcommon_string_ends_with_str (mcommon_string_append_destination (append), substring);
}

/**
 * @brief Test if the append destination has zero length
 * @param string mcommon_string_append_t with the string to test
 */
static BSON_INLINE bool
mcommon_string_append_is_empty (const mcommon_string_append_t *append)
{
   return mcommon_string_is_empty (mcommon_string_append_destination (append));
}

/**
 * @brief Signal an explicit overflow during string append
 * @param append Append operation, initialized with mcommon_string_append_init
 *
 * Future calls to mcommon_string_append_status() return false, exactly as if an overlong append was attempted and
 * failed. This should be used for cases when a logical overflow is occurring but it was detected early enough that no
 * actual append was attempted.
 */
static BSON_INLINE void
mcommon_string_append_set_overflow (mcommon_string_append_t *append)
{
   append->_max_len_exceeded = true;
}

/**
 * @brief Append selected characters from a template
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param template UTF-8 string listing allowed characters in the desired order
 * @param selector UTF-8 string that chooses which template characters are appended
 * @param selector_len Length of the selector string, in bytes
 *
 * Sort and filter lists of option characters. The template should list all allowed options in their desired order.
 * This implementation does not support multi-byte template characters. ASSERTs that each template character is <=
 * '\x7f'. Selectors may contain untrusted data, template should not.
 */
bool
mcommon_string_append_selected_chars (mcommon_string_append_t *append,
                                      const char *template,
                                      const char *selector,
                                      size_t selector_len);

/**
 * @brief Append a string with known length to the mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param str String to append a copy of, should be valid UTF-8
 * @param len Length of 'str', in bytes
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 *
 * If the string must be truncated to fit in the limit set by mcommon_string_append_init, it will always be split
 * in-between UTF-8 code points.
 */
static BSON_INLINE bool
mcommon_string_append_bytes (mcommon_string_append_t *append, const char *str, uint32_t len)
{
   if (BSON_UNLIKELY (!mcommon_string_append_status (append))) {
      return false;
   }

   mcommon_string_t *string = append->_string;
   char *buffer = string->str;
   uint64_t alloc = (uint64_t) string->alloc;
   uint64_t old_len = (uint64_t) string->len;
   uint64_t max_len = (uint64_t) append->_max_len;
   uint64_t new_len = old_len + (uint64_t) len;
   uint64_t new_len_with_nul = new_len + 1;

   // Fast path: no truncation, no buffer growing
   if (BSON_LIKELY (new_len <= max_len && new_len_with_nul <= alloc)) {
      memcpy (buffer + old_len, str, len);
      buffer[new_len] = '\0';
      string->len = (uint32_t) new_len;
      return true;
   }

   // Other cases are not inlined
   return mcommon_string_append_bytes_internal (append, str, len);
}

/**
 * @brief Append a NUL-terminated UTF-8 string to the mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param str NUL-terminated string to append a copy of
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 *
 * If the string must be truncated to fit in the limit set by mcommon_string_append_init, it will always be split
 * in-between UTF-8 code points.
 */
static BSON_INLINE bool
mcommon_string_append (mcommon_string_append_t *append, const char *str)
{
   return mcommon_string_append_bytes (append, str, strlen (str));
}

/**
 * @brief Atomically append a string with known length to the mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param str UTF-8 string to append a copy of
 * @param len Length of 'str', in bytes
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 *
 * Atomic version of mcommon_string_append_bytes. If string does not fit completely, it is not truncated.
 * The destination string is only modified if the entire append operation can be completed.
 */
bool
mcommon_string_append_bytes_atomic (mcommon_string_append_t *append, const char *str, uint32_t len);

/**
 * @brief Atomically append a NUL-terminated UTF-8 string to the mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param str NUL-terminated UTF-8 sequence to append a copy of
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 *
 * Atomic version of mcommon_string_append. If string does not fit completely, it is not truncated.
 * The destination string is only modified if the entire append operation can be completed.
 */
static BSON_INLINE bool
mcommon_string_append_atomic (mcommon_string_append_t *append, const char *str)
{
   return mcommon_string_append_bytes_atomic (append, str, strlen (str));
}

/**
 * @brief Append base64 encoded bytes to an mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param bytes Bytes to be encoded
 * @param len Number of bytes to encoded
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 */
bool
mcommon_string_append_base64_encode (mcommon_string_append_t *append, const uint8_t *bytes, uint32_t len);

/**
 * @brief Append an ObjectId as a hex string
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param value bson_oid_t value to copy
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 */
bool
mcommon_string_append_oid_as_hex (mcommon_string_append_t *append, const bson_oid_t *value);

/**
 * @brief Append printf() formatted text to a mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param format printf() format string
 * @param ... Format string arguments
 * @returns true if the append operation has no permanent error status, and this operation has succeeded. false if the
 * max length has been surpassed or this printf() experienced an unrecoverable error.
 *
 * Writes the printf() result directly into the mcommon_string_t buffer, growing it as needed.
 *
 * If the string must be truncated to fit in the limit set by mcommon_string_append_init, it will always be split
 * in-between UTF-8 code points.
 */
bool
mcommon_string_append_printf (mcommon_string_append_t *append, const char *format, ...) BSON_GNUC_PRINTF (2, 3);

/**
 * @brief Variant of mcommon_string_append_printf() that takes a va_list
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param format printf() format string
 * @param args Format string arguments
 * @returns true if the append operation has no permanent error status, and this operation has succeeded. false if the
 * max length has been surpassed or this printf() experienced an unrecoverable error.
 *
 * Writes the printf() result directly into the mcommon_string_t buffer, growing it as needed.
 *
 * If the string must be truncated to fit in the limit set by mcommon_string_append_init, it will always be split
 * in-between UTF-8 code points.
 */
bool
mcommon_string_append_vprintf (mcommon_string_append_t *append, const char *format, va_list args)
   BSON_GNUC_PRINTF (2, 0);

/**
 * @brief Append one code point to a mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param unichar Code point to append, as a bson_unichar_t
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 *
 * Guaranteed not to truncate. The character will fully append or no change will be made.
 */
static BSON_INLINE bool
mcommon_string_append_unichar (mcommon_string_append_t *append, bson_unichar_t unichar)
{
   if (BSON_UNLIKELY (!mcommon_string_append_status (append))) {
      return false;
   }

   mcommon_string_t *string = append->_string;
   char *buffer = string->str;
   uint64_t alloc = (uint64_t) string->alloc;
   uint64_t old_len = (uint64_t) string->len;
   uint64_t max_len = (uint64_t) append->_max_len;

   // Fast path: single-byte character, no truncation, no buffer growing
   if (BSON_LIKELY (unichar <= 0x7f)) {
      uint64_t new_len = old_len + 1;
      uint64_t new_len_with_nul = new_len + 1;
      if (BSON_LIKELY (new_len <= max_len && new_len_with_nul <= alloc)) {
         buffer[old_len] = (char) unichar;
         buffer[new_len] = '\0';
         string->len = new_len;
         return true;
      }
   }

   // Other cases are not inlined
   return mcommon_string_append_unichar_internal (append, unichar);
}


#endif /* MONGO_C_DRIVER_COMMON_STRING_PRIVATE_H */
