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


#ifndef PYMONGOC_CLIENT_POOL_H
#define PYMONGOC_CLIENT_POOL_H


#include <bson.h>
#include <mongoc.h>


BSON_BEGIN_DECLS


#ifdef _POSIX_C_SOURCE
# undef _POSIX_C_SOURCE
#endif
#include <Python.h>


typedef struct {
   PyObject_HEAD
   mongoc_client_pool_t *client_pool;
} pymongoc_client_pool_t;


PyTypeObject *pymongoc_client_pool_get_type (void);


BSON_END_DECLS


#endif /* PYMONGOC_CLIENT_POOL_H */
