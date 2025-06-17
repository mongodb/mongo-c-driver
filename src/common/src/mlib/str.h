/**
 * @file mlib/str.h
 * @brief String handling utilities
 * @date 2025-04-30
 *
 * This file provides utilities for handling *sized* strings. That is, strings
 * that carry their size, and do not rely on null termination. These APIs also
 * do a lot more bounds checking than is found in `<string.h>`.
 *
 * @copyright Copyright 2009-present MongoDB, Inc.
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
#ifndef MLIB_STR_H_INCLUDED
#define MLIB_STR_H_INCLUDED

#include <mlib/cmp.h>
#include <mlib/test.h>
#include <mlib/config.h>

#include <stddef.h>

/**
 * @brief A simple non-owning string-view type.
 *
 * The viewed string can be treated as an array of `char`. It's pointed-to data
 * must not be freed or manipulated.
 *
 * @note The viewed string is NOT guaranteed to be null-terminated. It WILL
 * be null-terminated if: Directly created from a string literal, a C string, or
 * a null-terminated `mlib_str`.
 * @note The viewed string MAY contain nul (zero-value) characters, so using them
 * with C string APIs could truncate unexpectedly.
 */
typedef struct mlib_str_view {
   // Pointer to the first character in the string
   const char *data;
   // Length of the array pointed-to by `data`
   size_t len;
} mlib_str_view;

/**
 * @brief Create a `str_view` that views the given array of `char`
 *
 * @param data Pointer to the beginning of the
 * @param len Length of the new string-view
 */
static inline mlib_str_view
mlib_str_view_data (const char *data, size_t len)
{
   mlib_str_view ret;
   ret.data = data;
   ret.len = len;
   return ret;
}

/**
 * @brief Coerce an object to an mlib_str_view
 *
 * The object requires a `data` and `len` member
 */
#define mlib_str_view_from(X) mlib_str_view_data ((X).data, (X).len)

/**
 * @brief Create a `str_view` referring to the given null-terminated C string
 *
 * @param s Pointer to a C string. The length of the returned string is infered using `strlen`
 */
static inline mlib_str_view
mlib_cstring (const char *s)
{
   const size_t l = strlen (s);
   return mlib_str_view_data (s, l);
}

/**
 * @brief Compare two strings
 *
 * If called with two arguments behaves the same as `strcmp`. If called with
 * three arguments, the center argument should be an infix operator to perform
 * the semantic comparison.
 */
static inline enum mlib_cmp_result
mlib_str_cmp (mlib_str_view a, mlib_str_view b)
{
   // Use `strncmp` on the common prefix for the strings
   size_t l = a.len;
   if (b.len < l) {
      l = b.len;
   }
   // Use `memcmp`, not `strncmp`: We want to respect nul characters
   int r = memcmp (a.data, b.data, l);
   if (r) {
      // Not equal: Compare with zero to normalize to the cmp_result value
      return mlib_cmp (r, 0);
   }
   // Same prefixes, the ordering is now based on their length (longer string > shorter string)
   return mlib_cmp (a.len, b.len);
}

#define mlib_str_cmp(...) MLIB_ARGC_PICK (_mlib_str_cmp, __VA_ARGS__)
#define _mlib_str_cmp_argc_2(A, B) mlib_str_cmp (mlib_str_view_from (A), mlib_str_view_from (B))
#define _mlib_str_cmp_argc_3(A, Op, B) (_mlib_str_cmp_argc_2 (A, B) Op 0)

/**
 * @brief Create a new `str_view` that views a substring within another string
 *
 * @param s The original string view to be inspected
 * @param start The number of `char` to skip in `s`
 * @param len The length of the new string view (optional, default SIZE_MAX)
 *
 * The length of the string view is clamped to the characters available in `s`,
 * so passing a too-large value for `len` is well-defined.
 *
 * Callable as:
 *
 * - `mlib_substr(s, start)`
 * - `mlib_substr(s, start, len)`
 */
static inline mlib_str_view
mlib_substr (mlib_str_view s, size_t start, size_t len)
{
   mlib_check (start <= s.len);
   const size_t remain = s.len - start;
   if (len > remain) {
      len = remain;
   }
   return mlib_str_view_data (s.data + start, len);
}

#define mlib_substr(...) MLIB_ARGC_PICK (_mlib_substr, __VA_ARGS__)
#define _mlib_substr_argc_2(Str, Start) _mlib_substr_argc_3 (Str, Start, SIZE_MAX)
#define _mlib_substr_argc_3(Str, Start, Stop) mlib_substr (mlib_str_view_from (Str), Start, Stop)

/**
 * @brief Find the first occurrence of `needle` within `hay`, returning the zero-based index
 * if found.
 *
 * @param hay The string which is being scanned
 * @param needle The substring that we are searching to find
 * @param pos The start position of the search (optional, default zero)
 * @param count The number of characters to search in `hay` (optional, default SIZE_MAX)
 * @return size_t If found, the zero-based index of the first occurrence within
 *    the string. If not found, returns `SIZE_MAX`.
 *
 * Callable as:
 *
 * - `mlib_str_find(hay, needle)`
 * - `mlib_str_find(hay, needle, pos)`
 * - `mlib_str_find(hay, needle, pos, count)`
 */
static inline size_t
mlib_str_find (mlib_str_view hay, mlib_str_view const needle, size_t const pos, size_t const len)
{
   // Trim the hay according to our search window:
   hay = mlib_substr (hay, pos, len);

   // Larger needle can never exist within the smaller string:
   if (hay.len < needle.len) {
      return SIZE_MAX;
   }

   // Set the index at which we can stop searching early. This will never
   // overflow, because we guard against hay.len > needle.len
   size_t stop_idx = hay.len - needle.len;
   // Use "<=", because we do want to include the final search position
   for (size_t offset = 0; offset <= stop_idx; ++offset) {
      if (memcmp (hay.data + offset, needle.data, needle.len) == 0) {
         // Return the found position. Adjust by the start pos since we may
         // have trimmed the search window
         return offset + pos;
      }
   }

   // Nothing was found. Return SIZE_MAX to indicate the not-found
   return SIZE_MAX;
}

#define mlib_str_find(...) MLIB_ARGC_PICK (_mlib_str_find, __VA_ARGS__)
#define _mlib_str_find_argc_2(Hay, Needle) _mlib_str_find_argc_3 (Hay, Needle, 0)
#define _mlib_str_find_argc_3(Hay, Needle, Start) _mlib_str_find_argc_4 (Hay, Needle, Start, SIZE_MAX)
#define _mlib_str_find_argc_4(Hay, Needle, Start, Stop) \
   mlib_str_find (mlib_str_view_from (Hay), mlib_str_view_from (Needle), Start, Stop)

/**
 * @brief Split a single string view into two strings at the given position
 *
 * @param s The string to be split
 * @param pos The position at which the prefix string is ended
 * @param drop [optional] The number of characters to drop between the prefix and suffix
 * @param prefix [out] Updated to point to the part of the string before the split
 * @param suffix [out] Updated to point to the part of the string after the split
 *
 * `pos` and `drop` are clamped to the size of the input string.
 *
 * Callable as:
 *
 * - `mlib_str_split_at(s, pos,       prefix, suffix)`
 * - `mlib_str_split_at(s, pos, drop, prefix, suffix)`
 *
 * If either `prefix` or `suffix` is a null pointer, then they will be ignored
 */
static void
mlib_str_split_at (mlib_str_view s, size_t pos, size_t drop, mlib_str_view *prefix, mlib_str_view *suffix)
{
   // Clamp the split-start position
   if (pos > s.len) {
      pos = s.len;
   }
   // Save the prefix string
   if (prefix) {
      *prefix = mlib_substr (s, 0, pos);
   }
   // Save the suffix string
   if (suffix) {
      // The number of characters that remain after the prefix is removed
      const size_t remain = s.len - pos;
      // Clamp the number of chars to drop to not overrun the input string
      if (remain < drop) {
         drop = remain;
      }
      // The start position of the new string
      const size_t next_start = pos + drop;
      *suffix = mlib_substr (s, next_start, SIZE_MAX);
   }
}

#define mlib_str_split_at(...) MLIB_ARGC_PICK (_mlib_str_split_at, __VA_ARGS__)
#define _mlib_str_split_at_argc_4(Str, Pos, Prefix, Suffix) _mlib_str_split_at_argc_5 (Str, Pos, 0, Prefix, Suffix)
#define _mlib_str_split_at_argc_5(Str, Pos, Drop, Prefix, Suffix) \
   mlib_str_split_at (mlib_str_view_from (Str), Pos, Drop, Prefix, Suffix)

/**
 * @brief Split a string in two around the first occurrence of some infix string.
 *
 * @param s The string to be split in twain
 * @param infix The infix string to be searched for
 * @param prefix The part of the string that precedes the infix (nullable)
 * @param suffix The part of the string that follows the infix (nullable)
 * @return true If the infix was found
 * @return false Otherwise
 *
 * @note If `infix` does not occur in `s`, then `*prefix` will be set equal to `s`,
 * and `*suffix` will be made an empty string, as if the infix occurred at the end
 * of the string.
 */
static bool
mlib_str_split_around (mlib_str_view s, mlib_str_view infix, mlib_str_view *prefix, mlib_str_view *suffix)
{
   // Find the position of the infix. If it is not found, returns SIZE_MAX
   const size_t pos = mlib_str_find (s, infix);
   // Split at the infix, dropping as many characters as are in the infix. If
   // the `pos` is SIZE_MAX, then this call will clamp to the end of the string.
   mlib_str_split_at (s, pos, infix.len, prefix, suffix);
   // Return `true` if we found the infix, indicated by a not-SIZE_MAX `pos`
   return pos != SIZE_MAX;
}

#define mlib_str_split_around(Str, Infix, PrefixPtr, SuffixPtr) \
   mlib_str_split_around (mlib_str_view_from ((Str)), mlib_str_view_from ((Infix)), (PrefixPtr), (SuffixPtr))

/**
 * @brief Test whether the given string starts with the given prefix
 *
 * @param str The string to be tested
 * @param prefix The prefix to be searched for
 * @return true if-and-only-if `str` starts with `prefix`
 * @return false Otherwise
 */
static inline bool
mlib_str_starts_with (mlib_str_view str, mlib_str_view prefix)
{
   return mlib_str_find (str, prefix) == 0;
}

#define mlib_str_starts_with(Str, Prefix) \
   mlib_str_starts_with (mlib_str_view_from ((Str)), mlib_str_view_from ((Prefix)))

#endif // MLIB_STR_H_INCLUDED
