/*
 * Copyright 2013 MongoDB, Inc.
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


void
_mongoc_array_init (mongoc_array_t *array,
                    size_t          element_size)
{
   bson_return_if_fail(array);
   bson_return_if_fail(element_size);

   array->len = 0;
   array->element_size = element_size;
   array->allocated = 128;
   array->data = bson_malloc0(array->allocated);
}


void
_mongoc_array_destroy (mongoc_array_t *array)
{
   if (array && array->data) {
      bson_free(array->data);
   }
}


void
_mongoc_array_append_vals (mongoc_array_t *array,
                           const void     *data,
                           uint32_t   n_elements)
{
   size_t len;
   size_t off;
   size_t next_size;

   bson_return_if_fail(array);
   bson_return_if_fail(data);

   off = array->element_size * array->len;
   len = (size_t)n_elements * array->element_size;
   if ((off + len) > array->allocated) {
      next_size = bson_next_power_of_two(off + len);
      array->data = bson_realloc(array->data, next_size);
      array->allocated = next_size;
   }

   memcpy((uint8_t *)array->data + off, data, len);

   array->len += n_elements;
}
