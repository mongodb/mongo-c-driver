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


#include "mongoc-trace.h"
#include "pymongoc-cursor.h"


static void
pymongoc_cursor_tp_dealloc (PyObject *self)
{
   pymongoc_cursor_t *cursor = (pymongoc_cursor_t *)self;

   ENTRY;

   if (cursor->cursor) {
      mongoc_cursor_destroy (cursor->cursor);
      cursor->cursor = NULL;
   }

   self->ob_type->tp_free(self);

   EXIT;
}


static PyTypeObject pymongoc_cursor_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "pymongc.Cursor",          /*tp_name*/
    sizeof(pymongoc_cursor_t), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    pymongoc_cursor_tp_dealloc,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "A MongoDB Cursor.",       /*tp_doc*/
};


PyObject *
pymongoc_cursor_new (mongoc_cursor_t *cursor)
{
   pymongoc_cursor_t *pycursor;

   BSON_ASSERT (cursor);

   pycursor = (pymongoc_cursor_t *)PyType_GenericNew (&pymongoc_cursor_type,
                                                      NULL, NULL);
   if (!pycursor) {
      return NULL;
   }

   pycursor->cursor = cursor;

   return (PyObject *)pycursor;
}


PyTypeObject *
pymongoc_cursor_get_type (void)
{
   static bool initialized;

   if (!initialized) {
      if (PyType_Ready (&pymongoc_cursor_type) < 0) {
         return NULL;
      }
      initialized = true;
   }

   return &pymongoc_cursor_type;
}
