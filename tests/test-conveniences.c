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

#include "mongoc-array-private.h"

#include "test-conveniences.h"

#ifdef _WIN32
# define strcasecmp _stricmp
#endif

static bool gConveniencesInitialized = false;
static mongoc_array_t gTmpBsonArray;

static void test_conveniences_cleanup ();


static void
test_conveniences_init ()
{
   if (!gConveniencesInitialized) {
      _mongoc_array_init (&gTmpBsonArray, sizeof (bson_t *));
      atexit (test_conveniences_cleanup);
      gConveniencesInitialized = true;
   }
}


static void
test_conveniences_cleanup ()
{
   int i;
   bson_t *doc;

   if (gConveniencesInitialized) {
      for (i = 0; i < gTmpBsonArray.len; i++) {
         doc = _mongoc_array_index (&gTmpBsonArray, bson_t *, i);
         bson_destroy (doc);
      }

      _mongoc_array_destroy (&gTmpBsonArray);
   }
}


bson_t *
tmp_bson (const char *json)
{
   bson_error_t error;
   char *double_quoted;
   bson_t *doc;

   test_conveniences_init ();

   double_quoted = single_quotes_to_double (json);
   doc = bson_new_from_json ((const uint8_t *)double_quoted,
                             -1, &error);

   if (!doc) {
      fprintf (stderr, "%s\n", error.message);
      abort ();
   }

   _mongoc_array_append_val (&gTmpBsonArray, doc);

   bson_free (double_quoted);

   return doc;
}


bool
get_exists_operator (const bson_value_t *value,
                     bool               *exists);

const bson_value_t *find (const bson_iter_t *iter,
                          const char *key,
                          bool is_command,
                          bool is_first);

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
   const bson_value_t *value;
   bson_iter_t doc_iter;
   bool is_first = true;
   bool is_exists_operator;
   bool exists;
   const bson_value_t *doc_value;

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
      value = bson_iter_value (&pattern_iter);
      doc_value = find (&doc_iter, key, is_command, is_first);

      /* is value {"$exists": true} or {"$exists": false} ? */
      is_exists_operator = get_exists_operator (value, &exists);

      if (is_exists_operator) {
         if (exists != (bool) doc_value) {
            return false;
         }
      } else if (!doc_value) {
         return false;
      } else if (!bson_value_equal (value, doc_value)) {
         return false;
      }

      /* don't advance if next call may be for another key in the same subdoc,
       * or if we're skipping a pattern key that was {$exists: false}. */
      if (!strchr (key, '.') && !(is_exists_operator && !exists)) {
         bson_iter_next (&doc_iter);
      }

      is_first = false;
   }

   return true;
}


/*--------------------------------------------------------------------------
 *
 * find --
 *
 *       Find the value for a key.
 *
 * Returns:
 *       A value, or NULL if the key is not found.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const bson_value_t *
find (const bson_iter_t *iter,
      const char *key,
      bool is_command,
      bool is_first)
{
   bson_iter_t i2;
   bson_iter_t descendent;

   /* don't advance iter. */
   memcpy (&i2, iter, sizeof *iter);

   if (strchr (key, '.')) {
      if (!bson_iter_find_descendant (&i2, key, &descendent)) {
         return NULL;
      }

      return bson_iter_value (&descendent);
   } else if (is_command && is_first) {
      if (!bson_iter_find_case (&i2, key)) {
         return NULL;
      }
   } else if (!bson_iter_find (&i2, key)) {
      return NULL;
   }

   return bson_iter_value (&i2);
}


bool
bson_init_from_value (bson_t             *b,
                      const bson_value_t *v)
{
   assert (v->value_type == BSON_TYPE_ARRAY ||
           v->value_type == BSON_TYPE_DOCUMENT);

   return bson_init_static (b, v->value.v_doc.data, v->value.v_doc.data_len);
}


/*--------------------------------------------------------------------------
 *
 * get_exists_operator --
 *
 *       Is value a subdocument like {"$exists": bool}?
 *
 * Returns:
 *       True if the value is a subdocument with the first key "$exists".
 *
 * Side effects:
 *       If the function returns true, *exists is set to true or false,
 *       the value of the bool.
 *
 *--------------------------------------------------------------------------
 */

bool
get_exists_operator (const bson_value_t *value,
                     bool               *exists)
{
   bson_t bson;
   bson_iter_t iter;

   if (value->value_type == BSON_TYPE_DOCUMENT &&
       bson_init_from_value (&bson, value) &&
       bson_iter_init_find (&iter, &bson, "$exists")) {
      *exists = bson_iter_as_bool (&iter);
      return true;
   }

   return false;
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
