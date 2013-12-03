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


#define _GNU_SOURCE

#include "mongoc-cursor.h"
#include "mongoc-cursor-array-private.h"
#include "mongoc-cursor-private.h"
#include "mongoc-client-private.h"
#include "mongoc-counters-private.h"
#include "mongoc-error.h"
#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-trace.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cursor-array"


typedef struct
{
   const bson_t       *result;
   bson_bool_t         has_array;
   bson_iter_t         iter;
   bson_t              bson;
   bson_uint32_t       document_len;
   const bson_uint8_t *document;
} mongoc_cursor_array_t;


static void *
_mongoc_cursor_array_new (void)
{
   mongoc_cursor_array_t *arr;

   ENTRY;

   arr = bson_malloc0 (sizeof *arr);

   RETURN (arr);
}


void
_mongoc_cursor_array_destroy (mongoc_cursor_t *cursor)
{
   ENTRY;

   bson_free (cursor->interface_data);
   _mongoc_cursor_destroy (cursor);

   EXIT;
}


bson_bool_t
_mongoc_cursor_array_next (mongoc_cursor_t *cursor,
                           const bson_t   **bson)
{
   bson_bool_t ret = TRUE;
   mongoc_cursor_array_t *arr;
   bson_iter_t iter;

   ENTRY;

   arr = cursor->interface_data;
   *bson = NULL;

   if (!arr->has_array) {
      arr->has_array = TRUE;

      ret = _mongoc_cursor_next (cursor, &arr->result);

      if (!(ret &&
            bson_iter_init_find (&iter, arr->result, "result") &&
            BSON_ITER_HOLDS_ARRAY (&iter) &&
            bson_iter_recurse (&iter, &arr->iter) &&
            bson_iter_next (&arr->iter))) {
         ret = FALSE;
      }
   } else {
      ret = bson_iter_next (&arr->iter);
   }

   if (ret) {
      bson_iter_document (&arr->iter, &arr->document_len, &arr->document);
      bson_init_static (&arr->bson, arr->document, arr->document_len);

      *bson = &arr->bson;
   }

   RETURN (ret);
}


mongoc_cursor_t *
_mongoc_cursor_array_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_t *clone;

   ENTRY;

   clone = _mongoc_cursor_clone (cursor);
   _mongoc_cursor_array_init (clone);

   RETURN (clone);
}


bson_bool_t
_mongoc_cursor_array_more (mongoc_cursor_t *cursor)
{
   bson_bool_t ret;
   mongoc_cursor_array_t *arr;
   bson_iter_t iter;

   ENTRY;

   arr = cursor->interface_data;

   if (arr->has_array) {
      memcpy (&iter, &arr->iter, sizeof iter);

      ret = bson_iter_next (&iter);
   } else {
      ret = TRUE;
   }

   RETURN (ret);
}


static mongoc_cursor_interface_t gMongocCursorArray = {
   _mongoc_cursor_array_clone,
   _mongoc_cursor_array_destroy,
   _mongoc_cursor_array_more,
   _mongoc_cursor_array_next,
};


void
_mongoc_cursor_array_init (mongoc_cursor_t *cursor)
{
   ENTRY;

   cursor->interface_data = _mongoc_cursor_array_new ();

   memcpy (&cursor->interface, &gMongocCursorArray,
           sizeof (mongoc_cursor_interface_t));

   EXIT;
}


