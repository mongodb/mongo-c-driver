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


#ifndef PYMONGOC_CURSOR_H
#define PYMONGOC_CURSOR_H


#include <mongoc.h>


BSON_BEGIN_DECLS


#define pymongoc_cursor_check(o) (Py_TYPE(o) == pymongoc_cursor_get_type())


typedef struct
{
   PyObject_HEAD
   mongoc_cursor_t *cursor;
} pymongoc_cursor_t;


PyTypeObject *pymongoc_cursor_get_type (void) BSON_GNUC_CONST;
PyObject     *pymongoc_cursor_new      (mongoc_cursor_t *cursor);


BSON_END_DECLS


#endif /* PYMONGOC_CURSOR_H */
