/*
 * Copyright 2020-present MongoDB, Inc.
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

#include "bson-parser.h"

#include "test-conveniences.h"
#include "TestSuite.h"
#include "utlist.h"

typedef struct _bson_parser_entry_t {
   bson_type_t btype;
   bool optional;
   void *out;
   char *key;
   bool set;
   struct _bson_parser_entry_t *alternates;
   struct _bson_parser_entry_t *next;
} bson_parser_entry_t;

struct _bson_parser_t {
   bson_parser_entry_t *entries;
   bool allow_extra;
};

bson_parser_t *
bson_parser_new (void)
{
   bson_parser_t *parser;

   parser = bson_malloc0 (sizeof (bson_parser_t));
   return parser;
}

void
bson_parser_allow_extra (bson_parser_t *parser, bool val)
{
   parser->allow_extra = val;
}

static void
bson_parser_entry_destroy (bson_parser_entry_t *entry, bool with_parsed_fields)
{
   if (with_parsed_fields) {
      if (entry->btype == BSON_TYPE_DOCUMENT ||
          entry->btype == BSON_TYPE_ARRAY) {
         bson_t **out;

         out = (bson_t **) entry->out;
         bson_destroy (*out);
         *out = NULL;
      } else if (entry->btype == BSON_TYPE_BOOL) {
         bool **out;

         out = (bool **) entry->out;
         bson_free (*out);
         *out = NULL;
      } else if (entry->btype == BSON_TYPE_UTF8) {
         char **out;

         out = (char **) entry->out;
         bson_free (*out);
         *out = NULL;
      }
   }
   bson_free (entry->key);
   bson_free (entry);
}

/* bson_parser_destroy destroys all parsed fields by default. */
static void
bson_parser_destroy_helper (bson_parser_t *parser, bool with_parsed_fields)
{
   bson_parser_entry_t *entry, *tmp, *alternate, *tmp2;

   if (!parser) {
      return;
   }

   LL_FOREACH_SAFE (parser->entries, entry, tmp)
   {
      /* Destroy alternates. */
      LL_FOREACH_SAFE (entry->alternates, alternate, tmp2)
      {
         bson_parser_entry_destroy (alternate, with_parsed_fields);
      }
      bson_parser_entry_destroy (entry, with_parsed_fields);
   }
   bson_free (parser);
}

void
bson_parser_destroy (bson_parser_t *parser)
{
   bson_parser_destroy_helper (parser, false);
}

void
bson_parser_destroy_with_parsed_fields (bson_parser_t *parser)
{
   bson_parser_destroy_helper (parser, true);
}

static void
bson_parser_add_entry (bson_parser_t *parser,
                       const char *key,
                       void *out,
                       bson_type_t btype,
                       bool optional,
                       bool alternate)
{
   bson_parser_entry_t *e;
   bson_parser_entry_t *parent;

   e = bson_malloc0 (sizeof (*e));
   e->optional = optional;
   e->btype = btype;
   e->out = out;
   e->key = bson_strdup (key);

   if (alternate) {
      /* There should already be an entry. Add this to the list of alternates.
       */
      LL_FOREACH (parser->entries, parent)
      {
         if (0 == strcmp (parent->key, key)) {
            LL_PREPEND (parent->alternates, e);
            return;
         }
      }
      test_error ("Invalid parser configuration. Attempted to add alternative "
                  "type for %s, but no type existed",
                  key);
   } else {
      LL_FOREACH (parser->entries, parent)
      {
         if (0 == strcmp (parent->key, key)) {
            test_error (
               "Invalid parser configuration. Attempted to add duplicated type "
               "for %s. If an alternate is desired, use *_alternate() helper",
               key);
         }
      }
   }

   LL_PREPEND (parser->entries, e);
}

void
bson_parser_utf8 (bson_parser_t *parser, const char *key, char **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_UTF8, false, false);
}

void
bson_parser_utf8_optional (bson_parser_t *parser, const char *key, char **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_UTF8, true, false);
}

void
bson_parser_utf8_alternate (bson_parser_t *parser, const char *key, char **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_UTF8, false, true);
}

void
bson_parser_doc (bson_parser_t *parser, const char *key, bson_t **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_DOCUMENT, false, false);
}

void
bson_parser_doc_optional (bson_parser_t *parser, const char *key, bson_t **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_DOCUMENT, true, false);
}

void
bson_parser_doc_alternate (bson_parser_t *parser, const char *key, bson_t **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_DOCUMENT, false, true);
}

void
bson_parser_array (bson_parser_t *parser, const char *key, bson_t **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_ARRAY, false, false);
}
void
bson_parser_array_optional (bson_parser_t *parser,
                            const char *key,
                            bson_t **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_ARRAY, true, false);
}

void
bson_parser_array_alternate (bson_parser_t *parser,
                             const char *key,
                             bson_t **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_ARRAY, false, true);
}

void
bson_parser_bool (bson_parser_t *parser, const char *key, bool **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_BOOL, false, false);
}

void
bson_parser_bool_optional (bson_parser_t *parser, const char *key, bool **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_BOOL, true, false);
}

void
bson_parser_bool_alternate (bson_parser_t *parser, const char *key, bool **out)
{
   *out = NULL;
   bson_parser_add_entry (
      parser, key, (void *) out, BSON_TYPE_BOOL, false, true);
}

bool
bson_parser_parse (bson_parser_t *parser, bson_t *in, bson_error_t *error)
{
   bson_iter_t iter;
   bson_parser_entry_t *entry;

   BSON_FOREACH (in, iter)
   {
      const char *key = bson_iter_key (&iter);
      bool matched = false;

      /* Check for a corresponding entry. */
      LL_FOREACH (parser->entries, entry)
      {
         if (0 == strcmp (entry->key, key)) {
            bson_type_t iter_type;
            bson_parser_entry_t *matching_entry = NULL;
            bson_string_t *expected_types;

            iter_type = bson_iter_type (&iter);
            expected_types =
               bson_string_new (_mongoc_bson_type_to_str (entry->btype));
            if (iter_type == entry->btype) {
               matching_entry = entry;
            } else {
               /* Attempt to find a matching alternate. */
               bson_parser_entry_t *alternate;

               LL_FOREACH (entry->alternates, alternate)
               {
                  bson_string_append_printf (
                     expected_types,
                     ",%s",
                     _mongoc_bson_type_to_str (alternate->btype));
                  if (iter_type == alternate->btype) {
                     matching_entry = alternate;
                     break;
                  }
               }
            }
            /* Find one such alternate with a matching type. */
            if (!matching_entry) {
               test_set_error (
                  error,
                  "error parsing bson, %s is type: %s, but wanted {%s}: "
                  "%s",
                  key,
                  _mongoc_bson_type_to_str (iter_type),
                  expected_types->str,
                  tmp_json (in));
               bson_string_free (expected_types, true);
               return false;
            }
            bson_string_free (expected_types, true);

            if (entry->btype == BSON_TYPE_UTF8) {
               char **out = (char **) entry->out;

               *out = bson_strdup (bson_iter_utf8 (&iter, NULL));
            } else if (entry->btype == BSON_TYPE_DOCUMENT ||
                       entry->btype == BSON_TYPE_ARRAY) {
               bson_t tmp;
               bson_t **out;

               out = (bson_t **) entry->out;
               bson_iter_bson (&iter, &tmp);
               *out = bson_copy (&tmp);
            } else if (entry->btype == BSON_TYPE_BOOL) {
               bool **out;

               out = (bool **) entry->out;
               *out = bson_malloc0 (sizeof (bool *));
               **out = bson_iter_bool (&iter);
            } else {
               test_error ("BSON type not implemented for parsing");
            }
            entry->set = true;
            matched = true;
            break;
         }
      }

      if (!matched && !parser->allow_extra) {
         test_set_error (
            error, "Extra field '%s' found parsing: %s", key, tmp_json (in));
         return false;
      }
   }

   /* Check if there are any unparsed required entries. */
   LL_FOREACH (parser->entries, entry)
   {
      if (!entry->optional && !entry->set) {
         test_set_error (error,
                         "Required field %s was not found parsing: %s",
                         entry->key,
                         tmp_json (in));
         return false;
      }
   }
   return true;
}

void
bson_parser_parse_or_assert (bson_parser_t *parser, bson_t *in)
{
   bson_error_t error;

   if (!bson_parser_parse (parser, in, &error)) {
      test_error ("Unable to parse: %s: %s", error.message, tmp_json (in));
   }
}
