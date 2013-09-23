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


#include <limits.h>

#include "mongoc-read-prefs.h"
#include "mongoc-read-prefs-private.h"


mongoc_read_prefs_t *
mongoc_read_prefs_new (mongoc_read_mode_t mode)
{
   mongoc_read_prefs_t *read_prefs;

   read_prefs = bson_malloc0(sizeof *read_prefs);
   read_prefs->mode = mode;
   bson_init(&read_prefs->tags);

   return read_prefs;
}


mongoc_read_mode_t
mongoc_read_prefs_get_mode (const mongoc_read_prefs_t *read_prefs)
{
   bson_return_val_if_fail(read_prefs, 0);
   return read_prefs->mode;
}


void
mongoc_read_prefs_set_mode (mongoc_read_prefs_t *read_prefs,
                            mongoc_read_mode_t   mode)
{
   bson_return_if_fail(read_prefs);
   bson_return_if_fail(mode <= MONGOC_READ_NEAREST);

   read_prefs->mode = mode;
}


const bson_t *
mongoc_read_prefs_get_tags (const mongoc_read_prefs_t *read_prefs)
{
   bson_return_val_if_fail(read_prefs, NULL);
   return &read_prefs->tags;
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


bson_bool_t
mongoc_read_prefs_is_valid (const mongoc_read_prefs_t *read_prefs)
{
   bson_return_val_if_fail(read_prefs, FALSE);

   /*
    * Tags are not supported with PRIMARY mode.
    */
   if (read_prefs->mode == MONGOC_READ_PRIMARY) {
      if (!bson_empty(&read_prefs->tags)) {
         return FALSE;
      }
   }

   return TRUE;
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


static int
_score_tags (const bson_t *read_tags,
             const bson_t *node_tags)
{
   bson_uint32_t len;
   bson_iter_t iter;
   const char *key;
   const char *str;
   int count;
   int i;

   bson_return_val_if_fail(read_tags, -1);
   bson_return_val_if_fail(node_tags, -1);

   count = bson_count_keys(read_tags);

   if (!bson_empty(read_tags) && bson_iter_init(&iter, read_tags)) {
      for (i = count; bson_iter_next(&iter); i--) {
         if (BSON_ITER_HOLDS_UTF8(&iter)) {
            key = bson_iter_key(&iter);
            str = bson_iter_utf8(&iter, &len);
            if (_contains_tag(node_tags, key, str, len)) {
               return count;
            }
         }
      }
      return -1;
   }

   return 0;
}


static int
_mongoc_read_prefs_score_primary (const mongoc_read_prefs_t   *read_prefs,
                                  const mongoc_cluster_node_t *node)
{
   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);
   return node->primary ? INT_MAX : 0;
}


static int
_mongoc_read_prefs_score_primary_preferred (const mongoc_read_prefs_t   *read_prefs,
                                            const mongoc_cluster_node_t *node)
{
   const bson_t *node_tags;
   const bson_t *read_tags;

   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);

   if (node->primary) {
      return INT_MAX;
   }

   node_tags = &node->tags;
   read_tags = &read_prefs->tags;

   return bson_empty(read_tags) ? 1 : _score_tags(read_tags, node_tags);
}


static int
_mongoc_read_prefs_score_secondary (const mongoc_read_prefs_t   *read_prefs,
                                    const mongoc_cluster_node_t *node)
{
   const bson_t *node_tags;
   const bson_t *read_tags;

   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);

   if (node->primary) {
      return -1;
   }

   node_tags = &node->tags;
   read_tags = &read_prefs->tags;

   return bson_empty(read_tags) ? 1 : _score_tags(read_tags, node_tags);
}


static int
_mongoc_read_prefs_score_secondary_preferred (const mongoc_read_prefs_t   *read_prefs,
                                              const mongoc_cluster_node_t *node)
{
   const bson_t *node_tags;
   const bson_t *read_tags;

   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);

   if (node->primary) {
      return 0;
   }

   node_tags = &node->tags;
   read_tags = &read_prefs->tags;

   return bson_empty(read_tags) ? 1 : _score_tags(read_tags, node_tags);
}


static int
_mongoc_read_prefs_score_nearest (const mongoc_read_prefs_t   *read_prefs,
                                  const mongoc_cluster_node_t *node)
{
   const bson_t *read_tags;
   const bson_t *node_tags;

   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);

   node_tags = &node->tags;
   read_tags = &read_prefs->tags;

   return bson_empty(read_tags) ? 1 : _score_tags(read_tags, node_tags);
}


int
_mongoc_read_prefs_score (const mongoc_read_prefs_t   *read_prefs,
                          const mongoc_cluster_node_t *node)
{
   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);

   switch (read_prefs->mode) {
   case MONGOC_READ_PRIMARY:
      return _mongoc_read_prefs_score_primary(read_prefs, node);
   case MONGOC_READ_PRIMARY_PREFERRED:
      return _mongoc_read_prefs_score_primary_preferred(read_prefs, node);
   case MONGOC_READ_SECONDARY:
      return _mongoc_read_prefs_score_secondary(read_prefs, node);
   case MONGOC_READ_SECONDARY_PREFERRED:
      return _mongoc_read_prefs_score_secondary_preferred(read_prefs, node);
   case MONGOC_READ_NEAREST:
      return _mongoc_read_prefs_score_nearest(read_prefs, node);
   default:
      BSON_ASSERT(FALSE);
      return -1;
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


mongoc_read_prefs_t *
mongoc_read_prefs_copy (const mongoc_read_prefs_t *read_prefs)
{
   mongoc_read_prefs_t *ret = NULL;

   if (read_prefs) {
      ret = mongoc_read_prefs_new(read_prefs->mode);
      bson_copy_to(&read_prefs->tags, &ret->tags);
   }

   return ret;
}
