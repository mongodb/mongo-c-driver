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


#ifndef MONGOC_ARRAY_PRIVATE_H
#define MONGOC_ARRAY_PRIVATE_H


#include <bson.h>


BSON_BEGIN_DECLS


typedef struct _mongoc_array_t mongoc_array_t;


struct _mongoc_array_t
{
   size_t  len;
   size_t  element_size;
   size_t  allocated;
   void   *data;
};


#define mongoc_array_append_val(a, v) mongoc_array_append_vals(a, &v, 1)
#define mongoc_array_index(a, t, i)   ((t)((t*)(a)->data)[i])


mongoc_array_t *mongoc_array_new         (size_t element_size);
void            mongoc_array_append_vals (mongoc_array_t *array,
                                          const void     *data,
                                          bson_uint32_t   n_elements);
void            mongoc_array_destroy     (mongoc_array_t *array);


BSON_END_DECLS


#endif /* MONGOC_ARRAY_PRIVATE_H */
