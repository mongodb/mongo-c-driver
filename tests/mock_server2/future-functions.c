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


#include "future-functions.h"


void static *background_bulk_operation_execute (void *data)
{
   future_t *future = (future_t *) data;
   future_t *copy = future_new_copy (future);
   future_value_t return_value;

   future_value_set_uint32_t (
         &return_value,
         mongoc_bulk_operation_execute (
               future_value_get_mongoc_bulk_operation_ptr (&copy->argv[0]),
               future_value_get_bson_ptr (&copy->argv[1]),
               future_value_get_bson_error_ptr (&copy->argv[2])));

   future_destroy (copy);
   future_resolve (future, return_value);

   return NULL;
}


future_t *future_bulk_operation_execute (mongoc_bulk_operation_t *bulk,
                                         bson_t *reply,
                                         bson_error_t *error)
{
   future_t *future;

   future = future_new (3);

   /* TODO: use setters */
   future->return_value.type = future_value_uint32_t_type;

   future->argv[0].type = future_value_mongoc_bulk_operation_ptr_type;
   future->argv[0].mongoc_bulk_operation_ptr_value = bulk;

   future->argv[1].type = future_value_bson_ptr_type;
   future->argv[1].bson_ptr_value = reply;

   future->argv[2].type = future_value_bson_error_ptr_type;
   future->argv[2].bson_error_ptr_value = error;

   future_start (future, background_bulk_operation_execute);

   return future;
}
