/*
 * Copyright 2014 MongoDB, Inc.
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


#ifndef PYMONGOC_URI_H
#define PYMONGOC_URI_H


#include <mongoc.h>


BSON_BEGIN_DECLS


typedef struct {
   PyObject_HEAD
   mongoc_uri_t *uri;
} pymongoc_uri_t;


PyTypeObject *pymongoc_uri_get_type (void);


BSON_END_DECLS


#endif /* PYMONGOC_URI_H */
