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


#ifndef MONGOC_HOST_LIST_H
#define MONGOC_HOST_LIST_H


#include <bson.h>


BSON_BEGIN_DECLS


typedef struct _mongoc_host_list_t mongoc_host_list_t;


struct _mongoc_host_list_t
{
   mongoc_host_list_t *next;
   char               *host_and_port;
   char               *host;
   bson_uint16_t       port;
   int                 family;
   void               *padding[4];
};


BSON_END_DECLS


#endif /* MONGOC_HOST_LIST_H */
