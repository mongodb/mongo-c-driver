/*
 * Copyright 2013 10gen Inc.
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


#include "mongoc-read-prefs.h"
#include "mongoc-read-prefs-private.h"


mongoc_read_prefs_t *
mongoc_read_prefs_new (void)
{
   mongoc_read_prefs_t *read_prefs;


   read_prefs = bson_malloc0(sizeof *read_prefs);
   read_prefs->mode = MONGOC_READ_PRIMARY;
   bson_init(&read_prefs->tags);

   return read_prefs;
}


void
mongoc_read_prefs_set_mode (mongoc_read_prefs_t *read_prefs,
                            mongoc_read_mode_t   mode)
{
   bson_return_if_fail(read_prefs);
   bson_return_if_fail(mode <= MONGOC_READ_NEAREST);

   read_prefs->mode = mode;
}


void
mongoc_read_prefs_set_tags (mongoc_read_prefs_t *read_prefs,
                            const bson_t        *tags)
{
   bson_return_if_fail(read_prefs);

   bson_destroy(&read_prefs->tags);
   if (tags) {
      bson_copy_to(tags, &read_prefs->tags);
   } else {
      bson_init(&read_prefs->tags);
   }
}


static bson_bool_t
_contains_tag (const bson_t *b,
               const char   *key,
               const char   *value,
               size_t        value_len)
{
   bson_iter_t iter;

   bson_return_val_if_fail(b, FALSE);
   bson_return_val_if_fail(key, FALSE);
   bson_return_val_if_fail(value, FALSE);

   if (bson_iter_init_find(&iter, b, key) &&
       BSON_ITER_HOLDS_UTF8(&iter) &&
       !strncmp(value, bson_iter_utf8(&iter, NULL), value_len)) {
      return TRUE;
   }

   return FALSE;
}


/**
 * mongoc_read_prefs_accepts:
 * @read_prefs: A mongoc_read_prefs_t.
 * @node: A mongoc_cluster_node_t.
 *
 * This is an internal funcion.
 *
 * This function checks to see if the node can service a request with the
 * supplied read preferences.
 *
 * If the node can absolutely not support the read preference, then zero
 * is returned.
 *
 * If the node can service the request, 1 is returned.
 *
 * If the node can service the request, but only as a fallback, then 2 is
 * returned.
 *
 * Returns: 0, 1, or 2 based on the support of @node to service the
 *    read-preference.
 */
int
_mongoc_read_prefs_accepts (mongoc_read_prefs_t   *read_prefs,
                            mongoc_cluster_node_t *node)
{
   bson_uint32_t len;
   bson_iter_t iter;
   const char *value;

   bson_return_val_if_fail(read_prefs, FALSE);
   bson_return_val_if_fail(node, FALSE);

   /*
    * TODO: Verify tag arrays.
    */
   if (!bson_empty(&read_prefs->tags)) {
      if (bson_iter_init(&iter, &read_prefs->tags)) {
         while (bson_iter_next(&iter)) {
            if (BSON_ITER_HOLDS_UTF8(&iter)) {
               value = bson_iter_utf8(&iter, &len);
               if (!_contains_tag(&node->tags,
                                  bson_iter_key(&iter),
                                  value, len)) {
                  return 0;
               }
            }
         }
      }
   }

   switch (read_prefs->mode) {
   case MONGOC_READ_PRIMARY:
      return node->primary;
   case MONGOC_READ_PRIMARY_PREFERRED:
      return node->primary ? 1 : 2;
   case MONGOC_READ_SECONDARY:
      return !node->primary;
   case MONGOC_READ_SECONDARY_PREFERRED:
      return !node->primary ? 1 : 2;
   case MONGOC_READ_NEAREST:
      return 1;
   default:
      BSON_ASSERT(FALSE);
      return 0;
   }
}


void
mongoc_read_prefs_destroy (mongoc_read_prefs_t *read_prefs)
{
   if (read_prefs) {
      bson_destroy(&read_prefs->tags);
      bson_free(read_prefs);
   }
}
