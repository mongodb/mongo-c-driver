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


#include <limits.h>
#include <stdarg.h>

#include <bson/bson-compat.h>
#include <bson/bson-config.h>
#include <mlib/cmp.h>
#include <common-string-private.h>
#include <common-bits-private.h>
#include <bson/bson-memory.h>
#include <bson/bson-utf8.h>

#ifdef BSON_HAVE_STRINGS_H
#include <strings.h>
#else
#include <string.h>
#endif


bson_string_t *
bson_string_new (const char *str) /* IN */
{
   /* Compatibility wrapper; deprecated.
    * New mcommon_string behavior is to use power of two rounding for resize but not for initial allocation unless
    * extra capacity is explicitly requested. This emulates the old behavior, padding the allocation of all new strings.
    */
   size_t len = str ? strlen (str) : 0;
   BSON_ASSERT (mlib_in_range (uint32_t, len) && (uint32_t) len < UINT32_MAX);
   uint32_t alloc = mcommon_next_power_of_two_u32 ((uint32_t) len + 1);
   return (bson_string_t *) mcommon_string_new_with_capacity (str ? str : "", (uint32_t) len, alloc - 1);
}

char *
bson_string_free (bson_string_t *string, /* IN */
                  bool free_segment)     /* IN */
{
   // Compatibility wrapper; deprecated.
   if (free_segment) {
      mcommon_string_destroy ((mcommon_string_t *) string);
      return NULL;
   } else {
      return mcommon_string_destroy_with_steal ((mcommon_string_t *) string);
   }
}

void
bson_string_append (bson_string_t *string, /* IN */
                    const char *str)       /* IN */
{
   // Compatibility wrapper; deprecated.
   BSON_ASSERT_PARAM (string);
   BSON_ASSERT_PARAM (str);

   mcommon_string_append_t append;
   mcommon_string_set_append ((mcommon_string_t *) string, &append);
   (void) mcommon_string_append (&append, str);
}

void
bson_string_append_unichar (bson_string_t *string,  /* IN */
                            bson_unichar_t unichar) /* IN */
{
   // Compatibility wrapper; deprecated.
   BSON_ASSERT_PARAM (string);

   mcommon_string_append_t append;
   mcommon_string_set_append ((mcommon_string_t *) string, &append);
   (void) mcommon_string_append_unichar (&append, unichar);
}

void
bson_string_append_c (bson_string_t *string, /* IN */
                      char c)                /* IN */
{
   // Compatibility wrapper; deprecated.
   BSON_ASSERT_PARAM (string);

   mcommon_string_append_t append;
   mcommon_string_set_append ((mcommon_string_t *) string, &append);
   (void) mcommon_string_append_bytes (&append, &c, 1);
}

void
bson_string_append_printf (bson_string_t *string, const char *format, ...)
{
   // Compatibility wrapper; deprecated.
   BSON_ASSERT_PARAM (string);
   BSON_ASSERT_PARAM (format);

   va_list args;
   mcommon_string_append_t append;
   mcommon_string_set_append ((mcommon_string_t *) string, &append);
   va_start (args, format);
   (void) mcommon_string_append_vprintf (&append, format, args);
   va_end (args);
}


void
bson_string_truncate (bson_string_t *string, /* IN */
                      uint32_t len)          /* IN */
{
   /* Does not preserve UTF-8 validity; deprecated.
    * Although the documentation only describes truncation as decreasing the length, we have undocumented requirements:
    * the string may grow or shrink, and the buffer is expected to be allocated using the same power-of-two scheme as
    * when growing to append. No effect if 'string' already has the requested length, regardless of the allocation size.
    * When extending string length, this implementation is guaranteed to fill with NUL bytes. Previous versions left the
    * new buffer contents undefined.
    */
   BSON_ASSERT_PARAM (string);
   BSON_ASSERT (len < UINT32_MAX);

   uint32_t old_len = string->len;
   if (len != old_len) {
      uint32_t alloc = mcommon_next_power_of_two_u32 (len + 1u);
      char *buffer = bson_realloc (string->str, alloc);
      string->str = buffer;
      string->alloc = alloc;
      string->len = len;

      if (len < old_len) {
         buffer[len] = '\0';
      } else {
         memset (buffer + old_len, 0, len + 1 - old_len);
      }
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strdup --
 *
 *       Portable strdup().
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
bson_strdup (const char *str) /* IN */
{
   long len;
   char *out;

   if (!str) {
      return NULL;
   }

   len = (long) strlen (str);
   out = bson_malloc (len + 1);

   if (!out) {
      return NULL;
   }

   memcpy (out, str, len + 1);

   return out;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strdupv_printf --
 *
 *       Like bson_strdup_printf() but takes a va_list.
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
bson_strdupv_printf (const char *format, /* IN */
                     va_list args)       /* IN */
{
   va_list my_args;
   char *buf;
   int len = 32;
   int n;

   BSON_ASSERT (format);

   buf = bson_malloc0 (len);

   while (true) {
      va_copy (my_args, args);
      n = bson_vsnprintf (buf, len, format, my_args);
      va_end (my_args);

      if (n > -1 && n < len) {
         return buf;
      }

      if (n > -1) {
         len = n + 1;
      } else {
         len *= 2;
      }

      buf = bson_realloc (buf, len);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strdup_printf --
 *
 *       Convenience function that formats a string according to @format
 *       and returns a copy of it.
 *
 * Returns:
 *       A newly created string that should be freed with bson_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_strdup_printf (const char *format, /* IN */
                    ...)                /* IN */
{
   va_list args;
   char *ret;

   BSON_ASSERT (format);

   va_start (args, format);
   ret = bson_strdupv_printf (format, args);
   va_end (args);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strndup --
 *
 *       A portable strndup().
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
bson_strndup (const char *str, /* IN */
              size_t n_bytes)  /* IN */
{
   char *ret;

   BSON_ASSERT (str);

   ret = bson_malloc (n_bytes + 1);
   bson_strncpy (ret, str, n_bytes + 1);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strfreev --
 *
 *       Frees each string in a NULL terminated array of strings.
 *       This also frees the underlying array.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_strfreev (char **str) /* IN */
{
   if (str) {
      for (char **ptr = str; *ptr != NULL; ++ptr) {
         bson_free (*ptr);
      }

      bson_free (str);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strnlen --
 *
 *       A portable strnlen().
 *
 * Returns:
 *       The length of @s up to @maxlen.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

size_t
bson_strnlen (const char *s, /* IN */
              size_t maxlen) /* IN */
{
#ifdef BSON_HAVE_STRNLEN
   return strnlen (s, maxlen);
#else
   size_t i;

   for (i = 0; i < maxlen; i++) {
      if (s[i] == '\0') {
         return i;
      }
   }

   return maxlen;
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strncpy --
 *
 *       A portable strncpy.
 *
 *       Copies @src into @dst, which must be @size bytes or larger.
 *       The result is guaranteed to be \0 terminated.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_strncpy (char *dst,       /* IN */
              const char *src, /* IN */
              size_t size)     /* IN */
{
   if (size == 0) {
      return;
   }

/* Prefer strncpy_s for MSVC, or strlcpy, which has additional checks and only
 * adds one trailing \0 */
#ifdef _MSC_VER
   strncpy_s (dst, size, src, _TRUNCATE);
#elif defined(BSON_HAVE_STRLCPY)
   strlcpy (dst, src, size);
#else
   strncpy (dst, src, size);
   dst[size - 1] = '\0';
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_vsnprintf --
 *
 *       A portable vsnprintf.
 *
 *       If more than @size bytes are required (exluding the null byte),
 *       then @size bytes will be written to @string and the return value
 *       is the number of bytes required.
 *
 *       This function will always return a NULL terminated string.
 *
 * Returns:
 *       The number of bytes required for @format excluding the null byte.
 *
 * Side effects:
 *       @str is initialized with the formatted string.
 *
 *--------------------------------------------------------------------------
 */

int
bson_vsnprintf (char *str,          /* IN */
                size_t size,        /* IN */
                const char *format, /* IN */
                va_list ap)         /* IN */
{
#ifdef _MSC_VER
   int r = -1;

   BSON_ASSERT (str);

   if (size == 0) {
      return 0;
   }

   r = _vsnprintf_s (str, size, _TRUNCATE, format, ap);
   if (r == -1) {
      r = _vscprintf (format, ap);
   }

   str[size - 1] = '\0';

   return r;
#else
   int r;

   BSON_ASSERT (str);

   if (size == 0) {
      return 0;
   }

   r = vsnprintf (str, size, format, ap);
   str[size - 1] = '\0';
   return r;
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_snprintf --
 *
 *       A portable snprintf.
 *
 *       If @format requires more than @size bytes, then @size bytes are
 *       written and the result is the number of bytes required (excluding
 *       the null byte).
 *
 *       This function will always return a NULL terminated string.
 *
 * Returns:
 *       The number of bytes required for @format.
 *
 * Side effects:
 *       @str is initialized.
 *
 *--------------------------------------------------------------------------
 */

int
bson_snprintf (char *str,          /* IN */
               size_t size,        /* IN */
               const char *format, /* IN */
               ...)
{
   int r;
   va_list ap;

   BSON_ASSERT (str);

   va_start (ap, format);
   r = bson_vsnprintf (str, size, format, ap);
   va_end (ap);

   return r;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_ascii_strtoll --
 *
 *       A portable strtoll.
 *
 *       Convert a string to a 64-bit signed integer according to the given
 *       @base, which must be 16, 10, or 8. Leading whitespace will be ignored.
 *
 *       If base is 0 is passed in, the base is inferred from the string's
 *       leading characters. Base-16 numbers start with "0x" or "0X", base-8
 *       numbers start with "0", base-10 numbers start with a digit from 1 to 9.
 *
 *       If @e is not NULL, it will be assigned the address of the first invalid
 *       character of @s, or its null terminating byte if the entire string was
 *       valid.
 *
 *       If an invalid value is encountered, errno will be set to EINVAL and
 *       zero will be returned. If the number is out of range, errno is set to
 *       ERANGE and LLONG_MAX or LLONG_MIN is returned.
 *
 * Returns:
 *       The result of the conversion.
 *
 * Side effects:
 *       errno will be set on error.
 *
 *--------------------------------------------------------------------------
 */

int64_t
bson_ascii_strtoll (const char *s, char **e, int base)
{
   char *tok = (char *) s;
   char *digits_start;
   char c;
   int64_t number = 0;
   int64_t sign = 1;
   int64_t cutoff;
   int64_t cutlim;

   errno = 0;

   if (!s) {
      errno = EINVAL;
      return 0;
   }

   c = *tok;

   while (bson_isspace (c)) {
      c = *++tok;
   }

   if (c == '-') {
      sign = -1;
      c = *++tok;
   } else if (c == '+') {
      c = *++tok;
   } else if (!isdigit (c)) {
      errno = EINVAL;
      return 0;
   }

   /* from here down, inspired by NetBSD's strtoll */
   if ((base == 0 || base == 16) && c == '0' && (tok[1] == 'x' || tok[1] == 'X')) {
      tok += 2;
      c = *tok;
      base = 16;
   }

   if (base == 0) {
      base = c == '0' ? 8 : 10;
   }

   /* Cutoff is the greatest magnitude we'll be able to multiply by base without
    * range error. If the current number is past cutoff and we see valid digit,
    * fail. If the number is *equal* to cutoff, then the next digit must be less
    * than cutlim, otherwise fail.
    */
   cutoff = sign == -1 ? INT64_MIN : INT64_MAX;
   cutlim = (int) (cutoff % base);
   cutoff /= base;
   if (sign == -1) {
      if (cutlim > 0) {
         cutlim -= base;
         cutoff += 1;
      }
      cutlim = -cutlim;
   }

   digits_start = tok;

   while ((c = *tok)) {
      if (isdigit (c)) {
         c -= '0';
      } else if (isalpha (c)) {
         c -= isupper (c) ? 'A' - 10 : 'a' - 10;
      } else {
         /* end of number string */
         break;
      }

      if (c >= base) {
         break;
      }

      if (sign == -1) {
         if (number < cutoff || (number == cutoff && c > cutlim)) {
            number = INT64_MIN;
            errno = ERANGE;
            break;
         } else {
            number *= base;
            number -= c;
         }
      } else {
         if (number > cutoff || (number == cutoff && c > cutlim)) {
            number = INT64_MAX;
            errno = ERANGE;
            break;
         } else {
            number *= base;
            number += c;
         }
      }

      tok++;
   }

   /* did we parse any digits at all? */
   if (e != NULL && tok > digits_start) {
      *e = tok;
   }

   return number;
}


int
bson_strcasecmp (const char *s1, const char *s2)
{
#ifdef BSON_OS_WIN32
   return _stricmp (s1, s2);
#else
   return strcasecmp (s1, s2);
#endif
}


bool
bson_isspace (int c)
{
   return c >= -1 && c <= 255 && isspace (c);
}
