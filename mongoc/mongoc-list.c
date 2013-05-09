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


#include "mongoc-list-private.h"


mongoc_list_t *
mongoc_list_append (mongoc_list_t *list,
                    void          *data)
{
   mongoc_list_t *item;
   mongoc_list_t *iter;

   item = bson_malloc0(sizeof *item);
   item->data = data;
   if (!list) {
      return item;
   }

   for (iter = list; iter->next; iter = iter->next) { }
   iter->next = item;

   return list;
}


mongoc_list_t *
mongoc_list_prepend (mongoc_list_t *list,
                     void          *data)
{
   mongoc_list_t *item;

   item = bson_malloc0(sizeof *item);
   item->data = data;
   item->next = list;

   return item;
}


mongoc_list_t *
mongoc_list_remove (mongoc_list_t *list,
                    void          *data)
{
   mongoc_list_t *iter;
   mongoc_list_t *prev = NULL;
   mongoc_list_t *ret = list;

   bson_return_val_if_fail(list, NULL);

   for (iter = list; iter; iter = iter->next) {
      if (iter->data == data) {
         if (iter != list) {
            prev->next = iter->next;
         } else {
            ret = iter->next;
         }
         bson_free(iter);
         break;
      }
      prev = iter;
   }

   return ret;
}


void
mongoc_list_foreach (mongoc_list_t *list,
                     void (*func) (void *data, void *user_data),
                     void          *user_data)
{
   mongoc_list_t *iter;

   bson_return_if_fail(func);

   for (iter = list; iter; iter = iter->next) {
      func(iter->data, user_data);
   }
}


void
mongoc_list_destroy (mongoc_list_t *list)
{
   mongoc_list_t *tmp = list;

   while (list) {
      tmp = list->next;
      bson_free(list);
      list = tmp;
   }
}
