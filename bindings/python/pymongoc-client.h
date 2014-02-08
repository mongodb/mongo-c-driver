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


#ifndef PYMONGOC_CLIENT_H
#define PYMONGOC_CLIENT_H


#include <bson.h>
#include <mongoc.h>

#ifdef _POSIX_C_SOURCE
# undef _POSIX_C_SOURCE
#endif
#include <Python.h>


BSON_BEGIN_DECLS


#define pymongoc_client_check(o) (Py_TYPE(o) == pymongoc_client_get_type())


typedef struct {
   PyObject_HEAD
   mongoc_client_t *client;
   bson_bool_t      owns_client;
} pymongoc_client_t;


PyTypeObject *pymongoc_client_get_type (void);
PyObject     *pymongoc_client_new      (mongoc_client_t *client,
                                        bson_bool_t      owns_client);


BSON_END_DECLS


#endif /* PYMONGOC_CLIENT_H */
