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


#include <string.h>

#include <bson/bson-memory.h>
#include <common-string-private.h>
#include <bson/bson-utf8.h>
#include <bson/bson-string-private.h>


/*
 *--------------------------------------------------------------------------
 *
 * _bson_utf8_get_sequence --
 *
 *       Determine the sequence length of the first UTF-8 character in
 *       @utf8. The sequence length is stored in @seq_length and the mask
 *       for the first character is stored in @first_mask.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @seq_length is set.
 *       @first_mask is set.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE void
_bson_utf8_get_sequence (const char *utf8,    /* IN */
                         uint8_t *seq_length, /* OUT */
                         uint8_t *first_mask) /* OUT */
{
   unsigned char c = *(const unsigned char *) utf8;
   uint8_t m;
   uint8_t n;

   /*
    * See the following[1] for a description of what the given multi-byte
    * sequences will be based on the bits set of the first byte. We also need
    * to mask the first byte based on that.  All subsequent bytes are masked
    * against 0x3F.
    *
    * [1] http://www.joelonsoftware.com/articles/Unicode.html
    */

   if ((c & 0x80) == 0) {
      n = 1;
      m = 0x7F;
   } else if ((c & 0xE0) == 0xC0) {
      n = 2;
      m = 0x1F;
   } else if ((c & 0xF0) == 0xE0) {
      n = 3;
      m = 0x0F;
   } else if ((c & 0xF8) == 0xF0) {
      n = 4;
      m = 0x07;
   } else {
      n = 0;
      m = 0;
   }

   *seq_length = n;
   *first_mask = m;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_validate --
 *
 *       Validates that @utf8 is a valid UTF-8 string. Note that we only
 *       support UTF-8 characters which have sequence length less than or equal
 *       to 4 bytes (RFC 3629).
 *
 *       If @allow_null is true, then \0 is allowed within @utf8_len bytes
 *       of @utf8.  Generally, this is bad practice since the main point of
 *       UTF-8 strings is that they can be used with strlen() and friends.
 *       However, some languages such as Python can send UTF-8 encoded
 *       strings with NUL's in them.
 *
 * Parameters:
 *       @utf8: A UTF-8 encoded string.
 *       @utf8_len: The length of @utf8 in bytes.
 *       @allow_null: If \0 is allowed within @utf8, excluding trailing \0.
 *
 * Returns:
 *       true if @utf8 is valid UTF-8. otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_utf8_validate (const char *utf8, /* IN */
                    size_t utf8_len,  /* IN */
                    bool allow_null)  /* IN */
{
   bson_unichar_t c;
   uint8_t first_mask;
   uint8_t seq_length;
   size_t i;
   size_t j;

   BSON_ASSERT (utf8);

   for (i = 0; i < utf8_len; i += seq_length) {
      _bson_utf8_get_sequence (&utf8[i], &seq_length, &first_mask);

      /*
       * Ensure we have a valid multi-byte sequence length.
       */
      if (!seq_length) {
         return false;
      }

      /*
       * Ensure we have enough bytes left.
       */
      if ((utf8_len - i) < seq_length) {
         return false;
      }

      /*
       * Also calculate the next char as a unichar so we can
       * check code ranges for non-shortest form.
       */
      c = utf8[i] & first_mask;

      /*
       * Check the high-bits for each additional sequence byte.
       */
      for (j = i + 1; j < (i + seq_length); j++) {
         c = (c << 6) | (utf8[j] & 0x3F);
         if ((utf8[j] & 0xC0) != 0x80) {
            return false;
         }
      }

      /*
       * Check for NULL bytes afterwards.
       *
       * Hint: if you want to optimize this function, starting here to do
       * this in the same pass as the data above would probably be a good
       * idea. You would add a branch into the inner loop, but save possibly
       * on cache-line bouncing on larger strings. Just a thought.
       */
      if (!allow_null) {
         for (j = 0; j < seq_length; j++) {
            if (((i + j) > utf8_len) || !utf8[i + j]) {
               return false;
            }
         }
      }

      /*
       * Code point won't fit in utf-16, not allowed.
       */
      if (c > 0x0010FFFF) {
         return false;
      }

      /*
       * Byte is in reserved range for UTF-16 high-marks
       * for surrogate pairs.
       */
      if ((c & 0xFFFFF800) == 0xD800) {
         return false;
      }

      /*
       * Check non-shortest form unicode.
       */
      switch (seq_length) {
      case 1:
         if (c <= 0x007F) {
            continue;
         }
         return false;

      case 2:
         if ((c >= 0x0080) && (c <= 0x07FF)) {
            continue;
         } else if (c == 0) {
            /* Two-byte representation for NULL. */
            if (!allow_null) {
               return false;
            }
            continue;
         }
         return false;

      case 3:
         if (((c >= 0x0800) && (c <= 0x0FFF)) || ((c >= 0x1000) && (c <= 0xFFFF))) {
            continue;
         }
         return false;

      case 4:
         if (((c >= 0x10000) && (c <= 0x3FFFF)) || ((c >= 0x40000) && (c <= 0xFFFFF)) ||
             ((c >= 0x100000) && (c <= 0x10FFFF))) {
            continue;
         }
         return false;

      default:
         return false;
      }
   }

   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * _is_special_char --
 *
 *       Uses a bit mask to check if a character requires special formatting
 *       or not. Called from bson_utf8_escape_for_json.
 *
 * Parameters:
 *       @c: An unsigned char c.
 *
 * Returns:
 *       true if @c requires special formatting. otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE bool
_is_special_char (unsigned char c)
{
   /*
   C++ equivalent:
   std::bitset<256> charmap = [...]
   return charmap[c];
   */
   static const bson_unichar_t charmap[8] = {0xffffffff, // control characters
                                             0x00000004, // double quote "
                                             0x10000000, // backslash
                                             0x00000000,
                                             0xffffffff,
                                             0xffffffff,
                                             0xffffffff,
                                             0xffffffff}; // non-ASCII
   const int int_index = ((int) c) / ((int) sizeof (bson_unichar_t) * 8);
   const int bit_index = ((int) c) & ((int) sizeof (bson_unichar_t) * 8 - 1);
   return ((charmap[int_index] >> bit_index) & ((bson_unichar_t) 1)) != 0u;
}

/*
 *--------------------------------------------------------------------------
 *
 * _bson_utf8_handle_special_char --
 *
 *       Appends a special character in the correct format when converting
 *       from UTF-8 to JSON. This includes characters that should be escaped
 *       as well as ASCII control characters.
 *
 *       Normal ASCII characters and multi-byte UTF-8 sequences are handled
 *       in bson_utf8_escape_for_json, where this function is called from.
 *
 * Parameters:
 *       @c: A uint8_t ASCII codepoint.
 *       @str: A string to append the special character to.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE void
_bson_utf8_handle_special_char (const uint8_t c,       /* IN */
                                mcommon_string_t *str) /* OUT */
{
   BSON_ASSERT (c < 0x80u);
   BSON_ASSERT (str);

   switch (c) {
   case '"':
      mcommon_string_append (str, "\\\"");
      break;
   case '\\':
      mcommon_string_append (str, "\\\\");
      break;
   case '\b':
      mcommon_string_append (str, "\\b");
      break;
   case '\f':
      mcommon_string_append (str, "\\f");
      break;
   case '\n':
      mcommon_string_append (str, "\\n");
      break;
   case '\r':
      mcommon_string_append (str, "\\r");
      break;
   case '\t':
      mcommon_string_append (str, "\\t");
      break;
   default: {
      // ASCII control character
      BSON_ASSERT (c < 0x20u);

      const char digits[] = "0123456789abcdef";
      char codepoint[6] = "\\u0000";

      codepoint[4] = digits[(c >> 4) & 0x0fu];
      codepoint[5] = digits[c & 0x0fu];

      _bson_string_append_ex (str, codepoint, sizeof (codepoint));
      break;
   }
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_escape_for_json --
 *
 *       Allocates a new string matching @utf8 except that special
 *       characters in JSON will be escaped. The resulting string is also
 *       UTF-8 encoded.
 *
 *       Both " and \ characters will be escaped. Additionally, if a NUL
 *       byte is found before @utf8_len bytes, it will be converted to the
 *       two byte UTF-8 sequence.
 *
 * Parameters:
 *       @utf8: A UTF-8 encoded string.
 *       @utf8_len: The length of @utf8 in bytes or -1 if NUL terminated.
 *
 * Returns:
 *       A newly allocated string that should be freed with bson_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_utf8_escape_for_json (const char *utf8, /* IN */
                           ssize_t utf8_len) /* IN */
{
   bool length_provided = true;
   size_t utf8_ulen;

   BSON_ASSERT (utf8);

   if (utf8_len < 0) {
      length_provided = false;
      utf8_ulen = strlen (utf8);
   } else {
      utf8_ulen = (size_t) utf8_len;
   }

   if (utf8_ulen == 0) {
      return bson_strdup ("");
   }

   const char *const end = utf8 + utf8_ulen;

   mcommon_string_t *const str = _bson_string_alloc (utf8_ulen);

   size_t normal_chars_seen = 0u;

   do {
      const uint8_t current_byte = (uint8_t) utf8[normal_chars_seen];
      if (!_is_special_char (current_byte)) {
         // Normal character, no need to do anything besides iterate
         // Copy rest of the string if we reach the end
         normal_chars_seen++;
         utf8_ulen--;
         if (utf8_ulen == 0) {
            _bson_string_append_ex (str, utf8, normal_chars_seen);
            break;
         }

         continue;
      }

      // Reached a special character. Copy over all of normal characters
      // we have passed so far
      if (normal_chars_seen > 0) {
         _bson_string_append_ex (str, utf8, normal_chars_seen);
         utf8 += normal_chars_seen;
         normal_chars_seen = 0;
      }

      // Check if expected char length goes past end
      // bson_utf8_get_char will crash without this check
      {
         uint8_t mask;
         uint8_t length_of_char;

         _bson_utf8_get_sequence (utf8, &length_of_char, &mask);
         if (utf8 > end - length_of_char) {
            goto invalid_utf8;
         }
      }

      // Check for null character
      // Null characters are only allowed if the length is provided
      if (utf8[0] == '\0' || (utf8[0] == '\xc0' && utf8[1] == '\x80')) {
         if (!length_provided) {
            goto invalid_utf8;
         }

         mcommon_string_append (str, "\\u0000");
         utf8_ulen -= *utf8 ? 2u : 1u;
         utf8 += *utf8 ? 2 : 1;
         continue;
      }

      // Multi-byte UTF-8 sequence
      if (current_byte > 0x7fu) {
         const char *utf8_old = utf8;
         size_t char_len;

         bson_unichar_t unichar = bson_utf8_get_char (utf8);

         if (!unichar) {
            goto invalid_utf8;
         }

         mcommon_string_append_unichar (str, unichar);
         utf8 = bson_utf8_next_char (utf8);

         char_len = (size_t) (utf8 - utf8_old);
         BSON_ASSERT (utf8_ulen >= char_len);
         utf8_ulen -= char_len;

         continue;
      }

      // Special ASCII characters (control chars and misc.)
      _bson_utf8_handle_special_char (current_byte, str);

      if (current_byte > 0) {
         utf8++;
      } else {
         goto invalid_utf8;
      }

      utf8_ulen--;
   } while (utf8_ulen > 0);

   return mcommon_string_free (str, false);

invalid_utf8:
   mcommon_string_free (str, true);
   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_get_char --
 *
 *       Fetches the next UTF-8 character from the UTF-8 sequence.
 *
 * Parameters:
 *       @utf8: A string containing validated UTF-8.
 *
 * Returns:
 *       A 32-bit bson_unichar_t reprsenting the multi-byte sequence.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_unichar_t
bson_utf8_get_char (const char *utf8) /* IN */
{
   bson_unichar_t c;
   uint8_t mask;
   uint8_t num;
   int i;

   BSON_ASSERT (utf8);

   _bson_utf8_get_sequence (utf8, &num, &mask);
   c = (*utf8) & mask;

   for (i = 1; i < num; i++) {
      c = (c << 6) | (utf8[i] & 0x3F);
   }

   return c;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_next_char --
 *
 *       Returns an incremented pointer to the beginning of the next
 *       multi-byte sequence in @utf8.
 *
 * Parameters:
 *       @utf8: A string containing validated UTF-8.
 *
 * Returns:
 *       An incremented pointer in @utf8.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_utf8_next_char (const char *utf8) /* IN */
{
   uint8_t mask;
   uint8_t num;

   BSON_ASSERT (utf8);

   _bson_utf8_get_sequence (utf8, &num, &mask);

   return utf8 + num;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_from_unichar --
 *
 *       Converts the unichar to a sequence of utf8 bytes and stores those
 *       in @utf8. The number of bytes in the sequence are stored in @len.
 *
 * Parameters:
 *       @unichar: A bson_unichar_t.
 *       @utf8: A location for the multi-byte sequence.
 *       @len: A location for number of bytes stored in @utf8.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @utf8 is set.
 *       @len is set.
 *
 *--------------------------------------------------------------------------
 */

void
bson_utf8_from_unichar (bson_unichar_t unichar,                      /* IN */
                        char utf8[BSON_ENSURE_ARRAY_PARAM_SIZE (6)], /* OUT */
                        uint32_t *len)                               /* OUT */
{
   BSON_ASSERT (utf8);
   BSON_ASSERT (len);

   if (unichar <= 0x7F) {
      utf8[0] = unichar;
      *len = 1;
   } else if (unichar <= 0x7FF) {
      *len = 2;
      utf8[0] = 0xC0 | ((unichar >> 6) & 0x3F);
      utf8[1] = 0x80 | ((unichar) & 0x3F);
   } else if (unichar <= 0xFFFF) {
      *len = 3;
      utf8[0] = 0xE0 | ((unichar >> 12) & 0xF);
      utf8[1] = 0x80 | ((unichar >> 6) & 0x3F);
      utf8[2] = 0x80 | ((unichar) & 0x3F);
   } else if (unichar <= 0x1FFFFF) {
      *len = 4;
      utf8[0] = 0xF0 | ((unichar >> 18) & 0x7);
      utf8[1] = 0x80 | ((unichar >> 12) & 0x3F);
      utf8[2] = 0x80 | ((unichar >> 6) & 0x3F);
      utf8[3] = 0x80 | ((unichar) & 0x3F);
   } else {
      *len = 0;
   }
}
