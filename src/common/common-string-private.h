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

#ifndef MCOMMON_STRING_H
#define MCOMMON_STRING_H

#include <bson/bson.h>
#include <common-macros-private.h> // BEGIN_IGNORE_DEPRECATIONS

// mcommon_string_t is an internal string type intended to replace the deprecated bson_string_t.
// When bson_string_t is removed, migrate the implementation to mcommon_string_t.
typedef bson_string_t mcommon_string_t;

static BSON_INLINE mcommon_string_t *
mcommon_string_new (const char *str)
{
   BEGIN_IGNORE_DEPRECATIONS
   return bson_string_new (str);
   END_IGNORE_DEPRECATIONS
}
static BSON_INLINE char *
mcommon_string_free (mcommon_string_t *string, bool free_segment)
{
   BEGIN_IGNORE_DEPRECATIONS
   return bson_string_free (string, free_segment);
   END_IGNORE_DEPRECATIONS
}
static BSON_INLINE void
mcommon_string_append (mcommon_string_t *string, const char *str)
{
   BEGIN_IGNORE_DEPRECATIONS
   bson_string_append (string, str);
   END_IGNORE_DEPRECATIONS
}
static BSON_INLINE void
mcommon_string_append_c (mcommon_string_t *string, char str)
{
   BEGIN_IGNORE_DEPRECATIONS
   bson_string_append_c (string, str);
   END_IGNORE_DEPRECATIONS
}
static BSON_INLINE void
mcommon_string_append_unichar (mcommon_string_t *string, bson_unichar_t unichar)
{
   BEGIN_IGNORE_DEPRECATIONS
   bson_string_append_unichar (string, unichar);
   END_IGNORE_DEPRECATIONS
}

static BSON_INLINE void
mcommon_string_append_printf (mcommon_string_t *string, const char *format, ...) BSON_GNUC_PRINTF (2, 3);

static BSON_INLINE void
mcommon_string_append_printf (mcommon_string_t *string, const char *format, ...)
{
   va_list args;
   char *ret;

   BSON_ASSERT_PARAM (string);
   BSON_ASSERT_PARAM (format);

   va_start (args, format);
   ret = bson_strdupv_printf (format, args);
   va_end (args);
   BEGIN_IGNORE_DEPRECATIONS
   bson_string_append (string, ret);
   END_IGNORE_DEPRECATIONS
   bson_free (ret);
}

static BSON_INLINE void
mcommon_string_truncate (mcommon_string_t *string, uint32_t len)
{
   BEGIN_IGNORE_DEPRECATIONS
   bson_string_truncate (string, len);
   END_IGNORE_DEPRECATIONS
}

#endif /* MCOMMON_STRING_H */
