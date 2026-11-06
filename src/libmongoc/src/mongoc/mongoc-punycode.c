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

#include <mongoc/mongoc-punycode-private.h>

#include <bson/bson.h>

#include <mlib/ckdint.h>
#include <mlib/str.h>

#include <stdint.h>

// Punycode (RFC 3492) parameterization.
enum {
   punycode_base = 36,
   punycode_tmin = 1,
   punycode_tmax = 26,
   punycode_skew = 38,
   punycode_damp = 700,
   punycode_initial_bias = 72,
   punycode_initial_n = 128u,
};

static char
_punycode_digit(uint32_t d)
{
   if (d < 26) {
      return (char)('a' + d);
   }
   return (char)('0' + d - 26);
}

static uint32_t
_punycode_adapt(uint32_t delta, uint32_t numpoints, bool firsttime)
{
   delta = firsttime ? delta / punycode_damp : delta / 2;
   delta += delta / numpoints;

   uint32_t k = 0;
   while (delta > ((punycode_base - punycode_tmin) * punycode_tmax) / 2) {
      delta /= punycode_base - punycode_tmin;
      k += punycode_base;
   }

   return k + (((punycode_base - punycode_tmin + 1) * delta) / (delta + punycode_skew));
}

static uint32_t
_punycode_threshold(uint32_t k, uint32_t bias)
{
   if (k <= bias) {
      return punycode_tmin;
   }
   if (k >= bias + punycode_tmax) {
      return punycode_tmax;
   }
   return k - bias;
}

// Decode one UTF-8 sequence. Returns the number of bytes consumed, or -1 on
// invalid input (including overlong encodings, surrogates, and out-of-range).
static ssize_t
_utf8_decode(const uint8_t *data, size_t len, uint32_t *out)
{
   BSON_ASSERT(len > 0);

   const uint8_t b0 = data[0];
   if (b0 < 0x80u) {
      *out = b0;
      return 1;
   }

   size_t expect = 0;
   uint32_t cp = 0;
   if ((b0 & 0xE0u) == 0xC0u) {
      expect = 1;
      cp = b0 & 0x1Fu;
   } else if ((b0 & 0xF0u) == 0xE0u) {
      expect = 2;
      cp = b0 & 0x0Fu;
   } else if ((b0 & 0xF8u) == 0xF0u) {
      expect = 3;
      cp = b0 & 0x07u;
   } else {
      return -1; // Continuation byte or invalid leading byte.
   }

   if (len < expect + 1) {
      return -1; // Truncated sequence.
   }

   for (size_t i = 1; i <= expect; i++) {
      const uint8_t b = data[i];
      if ((b & 0xC0u) != 0x80u) {
         return -1;
      }
      cp = (cp << 6) | (b & 0x3Fu);
   }

   // Reject overlong encodings and out-of-range code points.
   if ((expect == 1 && cp < 0x80u) || (expect == 2 && cp < 0x800u) || (expect == 3 && cp < 0x10000u) ||
       cp > 0x10FFFFu) {
      return -1;
   }
   // Reject surrogates (encoded UTF-8 must not contain them).
   if (cp >= 0xD800u && cp <= 0xDFFFu) {
      return -1;
   }

   *out = cp;
   return (ssize_t)(expect + 1);
}

bool
mongoc_punycode_encode_label(const char *label, size_t label_len, mstr *dst)
{
   BSON_ASSERT_PARAM(label);
   BSON_ASSERT_PARAM(dst);

   // Pre-reserve so dst's buffer is provably non-null before any append: dst
   // is caller-provided, and its state cannot be proven across the translation
   // unit boundary.
   mstr_reserve(dst, 64);

   // DNS labels are at most 63 octets, so a bounded code point buffer suffices.
   uint32_t codepoints[64];
   size_t ncodepoints = 0;

   {
      const uint8_t *cursor = (const uint8_t *)label;
      const uint8_t *const end = cursor + label_len;
      while (cursor < end) {
         uint32_t cp = 0;
         const ssize_t consumed = _utf8_decode(cursor, (size_t)(end - cursor), &cp);
         if (consumed < 0) {
            return false; // Not valid UTF-8.
         }
         if (ncodepoints == 64) {
            return false; // Exceeds a DNS label.
         }
         codepoints[ncodepoints++] = cp;
         cursor += (size_t)consumed;
      }
   }

   // Copy the basic code points (ASCII) as-is.
   size_t basic = 0;
   for (size_t i = 0; i < ncodepoints; i++) {
      if (codepoints[i] < 0x80u) {
         if (!mstr_append_char(dst, (char)codepoints[i])) {
            return false;
         }
         basic++;
      }
   }
   if (basic > 0) {
      if (!mstr_append_char(dst, '-')) {
         return false;
      }
   }

   uint32_t n = punycode_initial_n;
   uint32_t delta = 0;
   uint32_t bias = punycode_initial_bias;
   size_t handled = basic;

   while (handled < ncodepoints) {
      // Find the smallest code point >= n.
      uint32_t m = UINT32_MAX;
      for (size_t i = 0; i < ncodepoints; i++) {
         if (codepoints[i] >= n && codepoints[i] < m) {
            m = codepoints[i];
         }
      }
      if (m == UINT32_MAX) {
         return false;
      }

      // delta += (m - n) * (handled + 1), overflow-checked.
      {
         uint32_t step = 0;
         if (mlib_mul(&step, m - n, (uint32_t)(handled + 1u)) || mlib_add(&delta, delta, step)) {
            return false;
         }
      }
      n = m;

      for (size_t i = 0; i < ncodepoints; i++) {
         const uint32_t c = codepoints[i];
         if (c < n) {
            delta++;
            continue;
         }
         if (c != n) {
            continue;
         }

         // Emit the variable-length base-36 encoding of `delta`.
         uint32_t q = delta;
         for (uint32_t k = punycode_base;; k += punycode_base) {
            const uint32_t t = _punycode_threshold(k, bias);
            if (q < t) {
               break;
            }
            if (!mstr_append_char(dst, _punycode_digit(t + (q - t) % (punycode_base - t)))) {
               return false;
            }
            q = (q - t) / (punycode_base - t);
         }
         if (!mstr_append_char(dst, _punycode_digit(q))) {
            return false;
         }

         bias = _punycode_adapt(delta, (uint32_t)(handled + 1u), handled == basic);
         delta = 0;
         handled++;
      }

      // RFC 3492 section 6.3: "increment delta and n".
      delta++;
      n++;
   }

   return true;
}

bool
mongoc_punycode_canonicalize_domain(const char *domain, size_t domain_len, mstr *dst)
{
   BSON_ASSERT_PARAM(domain);
   BSON_ASSERT_PARAM(dst);

   if (domain_len == 0) {
      return false;
   }

   // Pre-reserve so dst's buffer is provably non-null before any append: dst
   // is caller-provided, and its state cannot be proven across the translation
   // unit boundary. The bound covers the dot separators plus the "xn--"
   // prefixes and digit expansion the labels can produce.
   mstr_reserve(dst, domain_len + 32);

   const char *start = domain;
   const char *const end = domain + domain_len;

   for (const char *c = domain;; c++) {
      if (*c == '.' || c == end) {
         const size_t label_len = (size_t)(c - start);
         if (label_len == 0) {
            if (c == end && dst->len > 0) {
               // A trailing dot merely terminates the domain; drop the
               // separator that was just appended for it.
               if (!mstr_resize(dst, dst->len - 1)) {
                  return false;
               }
               return true;
            }
            return false; // Leading or doubled dots.
         }

         // ASCII labels are lowercased in place; non-ASCII labels are
         // Punycode-encoded with the "xn--" prefix.
         bool ascii = true;
         for (size_t i = 0; i < label_len; i++) {
            if ((unsigned char)start[i] >= 0x80u) {
               ascii = false;
               break;
            }
         }
         if (ascii) {
            for (size_t i = 0; i < label_len; i++) {
               const char ch = (start[i] >= 'A' && start[i] <= 'Z') ? (char)(start[i] - 'A' + 'a') : start[i];
               if (!mstr_append_char(dst, ch)) {
                  return false;
               }
            }
         } else {
            if (!mstr_append(dst, mstr_view_data("xn--", 4))) {
               return false;
            }
            if (!mongoc_punycode_encode_label(start, label_len, dst)) {
               return false;
            }
         }

         if (c == end) {
            return true;
         }
         if (!mstr_append_char(dst, '.')) {
            return false;
         }
         start = c + 1;
      }
   }
}
