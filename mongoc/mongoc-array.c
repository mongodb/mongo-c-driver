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


#include "mongoc-array-private.h"


static BSON_INLINE bson_uint32_t
npow2 (bson_uint32_t v)
{
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   v++;

   return v;
}


mongoc_array_t *
mongoc_array_new (size_t element_size)
{
   mongoc_array_t *ar;

   bson_return_val_if_fail(element_size, NULL);

   ar = bson_malloc0(sizeof *ar);
   ar->len = 0;
   ar->element_size = element_size;
   ar->allocated = 128;
   ar->data = bson_malloc0(ar->allocated);

   return ar;
}


void
mongoc_array_destroy (mongoc_array_t *array)
{
   if (array) {
      bson_free(array->data);
      bson_free(array);
   }
}


void
mongoc_array_append_vals (mongoc_array_t *array,
                          const void     *data,
                          bson_uint32_t   n_elements)
{
   size_t len;
   size_t off;
   size_t next_size;

   bson_return_if_fail(array);
   bson_return_if_fail(data);

   off = array->element_size * array->len;
   len = (size_t)n_elements * array->element_size;
   if ((off + len) > array->allocated) {
      next_size = npow2(off + len);
      array->data = bson_realloc(array->data, next_size);
      array->allocated = next_size;
   }

   memcpy(array->data + off, data, len);

   array->len += n_elements;
}
