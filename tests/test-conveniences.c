/*
 * Copyright 2015 MongoDB, Inc.
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


#include <bson.h>

#include "test-conveniences.h"


bool
get_exists_operator (bson_iter_t *iter,
                     bool        *exists);

bool
bson_value_equal (const bson_value_t *a,
                  const bson_value_t *b);

bool
match_bson (const bson_t *doc,
            const bson_t *pattern,
            bool          is_command);


/*--------------------------------------------------------------------------
 *
 * single_quotes_to_double --
 *
 *       Copy str with single-quotes replaced by double.
 *
 * Returns:
 *       A string you must bson_free.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
single_quotes_to_double (const char *str)
{
   char *result = bson_strdup (str);
   char *p;

   for (p = result; *p; p++) {
      if (*p == '\'') {
         *p = '"';
      }
   }

   return result;
}


/*--------------------------------------------------------------------------
 *
 * match_json --
 *
 *       Call match_bson on "doc" and "json_pattern".
 *       For convenience, single-quotes are synonymous with double-quotes.
 *
 *       A NULL doc or NULL json_pattern means "{}".
 *
 * Returns:
 *       True or false.
 *
 * Side effects:
 *       Logs if no match.
 *
 *--------------------------------------------------------------------------
 */

bool
match_json (const bson_t *doc,
            const char   *json_pattern,
            bool          is_command,
            const char   *filename,
            int           lineno,
            const char   *funcname)
{
   char *double_quoted = single_quotes_to_double (
         json_pattern ? json_pattern : "{}");
   bson_error_t error;
   bson_t *pattern;
   bool matches;

   pattern = bson_new_from_json ((const uint8_t *) double_quoted, -1, &error);

   if (!pattern) {
      fprintf (stderr, "couldn't parse JSON: %s\n", error.message);
      abort ();
   }

   matches = match_bson (doc, pattern, is_command);

   if (!matches) {
      fprintf (stderr,
            "ASSERT_MATCH failed with document:\n\n"
                  "%s\n"
                  "pattern:\n%s\n\n"
                  "%s:%d %s()\n",
            doc ? bson_as_json (doc, NULL) : "{}",
            double_quoted,
            filename, lineno, funcname);
   }

   bson_destroy (pattern);
   bson_free (double_quoted);

   return matches;
}


/*--------------------------------------------------------------------------
 *
 * match_bson --
 *
 *       Does "doc" match "pattern"?
 *
 *       mongoc_matcher_t prohibits $-prefixed keys, which is something
 *       we need to test in e.g. test_mongoc_client_read_prefs, so this
 *       does *not* use mongoc_matcher_t. Instead, "doc" matches "pattern"
 *       if its key-value pairs are a simple superset of pattern's. Order
 *       matters. The only special pattern syntax is {"$exists": true/false}.
 *
 *       The first key matches case-insensitively if is_command.
 *
 *       A NULL doc or NULL pattern means "{}".
 *
 * Returns:
 *       True or false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
match_bson (const bson_t *doc,
            const bson_t *pattern,
            bool          is_command)
{
   bson_iter_t pattern_iter;
   const char *key;
   bson_iter_t doc_iter;
   bool first_pattern_key = true;
   bool exists;

   if (bson_empty0 (pattern)) {
      /* matches anything */
      return true;
   }

   if (bson_empty0 (doc)) {
      /* non-empty pattern can't match doc */
      return false;
   }

   assert (bson_iter_init (&pattern_iter, pattern));
   assert (bson_iter_init (&doc_iter, doc));

   while (bson_iter_next (&pattern_iter)) {
      key = bson_iter_key (&pattern_iter);
      if (first_pattern_key && is_command) {
         if (!bson_iter_next (&doc_iter) ||
             strcasecmp (key, bson_iter_key (&doc_iter))) {
            return false;
         }

         if (!bson_value_equal (bson_iter_value (&pattern_iter),
                                bson_iter_value (&doc_iter))) {
            return false;
         }
      } else {
         /* is pattern_iter at "key": {"$exists": bool} ? */
         if (get_exists_operator (&pattern_iter, &exists)) {
            if (exists != bson_has_field (doc, key)) {
               return false;
            }
         } else if (!bson_iter_find (&doc_iter, key)) {
            return false;
         } else if (!bson_value_equal (bson_iter_value (&pattern_iter),
                                       bson_iter_value (&doc_iter))) {
            return false;
         }
      }

      first_pattern_key = false;
   }

   return true;
}


/*--------------------------------------------------------------------------
 *
 * get_exists_operator --
 *
 *       Is iter at a subdocument value like {"$exists": bool}?
 *
 * Returns:
 *       True if the current value is a subdocument with the first key
 *       "$exists".
 *
 * Side effects:
 *       If the function returns true, *exists is set to true or false,
 *       the value of the bool.
 *
 *--------------------------------------------------------------------------
 */

bool
get_exists_operator (bson_iter_t *iter,
                     bool        *exists)
{
   bson_iter_t child;

   if (BSON_ITER_HOLDS_DOCUMENT (iter)) {
      if (bson_iter_recurse (iter, &child) &&
          bson_iter_next (&child) &&
          BSON_ITER_IS_KEY (&child, "$exists")) {
         *exists = bson_iter_as_bool (&child);
         return true;
      }
   }

   return false;
}


bool
bson_init_from_value (bson_t             *b,
                      const bson_value_t *v)
{
   assert (v->value_type == BSON_TYPE_ARRAY ||
           v->value_type == BSON_TYPE_DOCUMENT);

   return bson_init_static (b, v->value.v_doc.data, v->value.v_doc.data_len);
}


bool
match_bson_arrays (const bson_t *a,
                   const bson_t *b)
{
   /* an array is just a document with keys "0", "1", ...
    * so match_bson suffices if the number of keys is equal. */
   if (bson_count_keys (a) != bson_count_keys (b)) {
      return false;
   }
   
   return match_bson (a, b, false);
}


bool
bson_value_equal (const bson_value_t *a,
                  const bson_value_t *b)
{
   bson_t subdoc_a;
   bson_t subdoc_b;
   bool ret;

   if (a->value_type != b->value_type) {
      return false;
   }
   
   switch (a->value_type) {
   case BSON_TYPE_ARRAY:
   case BSON_TYPE_DOCUMENT:

      if (!bson_init_from_value (&subdoc_a, a)) {
         return false;
      }

      if (!bson_init_from_value (&subdoc_b, b)) {
         bson_destroy (&subdoc_a);
         return false;
      }

      if (a->value_type == BSON_TYPE_ARRAY) {
         ret = match_bson_arrays (&subdoc_a, &subdoc_b);
      } else {
         ret = match_bson (&subdoc_a, &subdoc_b, false);
      }

      bson_destroy (&subdoc_a);
      bson_destroy (&subdoc_b);

      return ret;

   case BSON_TYPE_BINARY:
      return a->value.v_binary.data_len == b->value.v_binary.data_len &&
             !memcmp (a->value.v_binary.data,
                      b->value.v_binary.data,
                      a->value.v_binary.data_len);

   case BSON_TYPE_BOOL:
      return a->value.v_bool == b->value.v_bool;

   case BSON_TYPE_CODE:
      return a->value.v_code.code_len == b->value.v_code.code_len &&
             !memcmp (a->value.v_code.code,
                      b->value.v_code.code,
                      a->value.v_code.code_len);

   case BSON_TYPE_CODEWSCOPE:
      return a->value.v_codewscope.code_len == b->value.v_codewscope.code_len &&
             !memcmp (a->value.v_codewscope.code,
                      b->value.v_codewscope.code,
                      a->value.v_codewscope.code_len) &&
             a->value.v_codewscope.scope_len == b->value.v_codewscope.scope_len
             && !memcmp (a->value.v_codewscope.scope_data,
                         b->value.v_codewscope.scope_data,
                         a->value.v_codewscope.scope_len);

   case BSON_TYPE_DATE_TIME:
      return a->value.v_datetime == b->value.v_datetime;
   case BSON_TYPE_DOUBLE:
      return a->value.v_double == b->value.v_double;
   case BSON_TYPE_INT32:
      return a->value.v_int32 == b->value.v_int32;
   case BSON_TYPE_INT64:
      return a->value.v_int64 == b->value.v_int64;
   case BSON_TYPE_OID:
      return bson_oid_equal (&a->value.v_oid, &b->value.v_oid);
   case BSON_TYPE_REGEX:
      return !strcmp (a->value.v_regex.regex, b->value.v_regex.regex) &&
             !strcmp (a->value.v_regex.options, b->value.v_regex.options);
   case BSON_TYPE_SYMBOL:
      return a->value.v_symbol.len == b->value.v_symbol.len &&
             !strncmp (a->value.v_symbol.symbol,
                       b->value.v_symbol.symbol,
                       a->value.v_symbol.len);
   case BSON_TYPE_TIMESTAMP:
      return a->value.v_timestamp.timestamp == b->value.v_timestamp.timestamp &&
             a->value.v_timestamp.increment == b->value.v_timestamp.increment;
   case BSON_TYPE_UTF8:
      return a->value.v_utf8.len == b->value.v_utf8.len &&
             !strncmp (a->value.v_utf8.str,
                       b->value.v_utf8.str,
                       a->value.v_utf8.len);

   /* these are empty types, if "a" and "b" are the same type they're equal */
   case BSON_TYPE_EOD:
   case BSON_TYPE_MAXKEY:
   case BSON_TYPE_MINKEY:
   case BSON_TYPE_NULL:
   case BSON_TYPE_UNDEFINED:
      return true;

   case BSON_TYPE_DBPOINTER:
      fprintf (stderr, "DBPointer comparison not implemented");
      abort ();

   default:
      fprintf (stderr, "unexpected value type %d", a->value_type);
      abort ();
   }
}
