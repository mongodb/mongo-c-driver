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
#include "pymongoc-database.h"


static void
pymongoc_database_tp_dealloc (PyObject *self)
{
   pymongoc_database_t *database = (pymongoc_database_t *)self;

   ENTRY;

   if (database->database) {
      mongoc_database_destroy (database->database);
      database->database = NULL;
   }

   self->ob_type->tp_free (self);

   EXIT;
}


static PyTypeObject pymongoc_database_type = {
    PyObject_HEAD_INIT(NULL)
    0,                           /*ob_size*/
    "pymongc.Database",          /*tp_name*/
    sizeof(pymongoc_database_t), /*tp_basicsize*/
    0,                           /*tp_itemsize*/
    pymongoc_database_tp_dealloc,/*tp_dealloc*/
    0,                           /*tp_print*/
    0,                           /*tp_getattr*/
    0,                           /*tp_setattr*/
    0,                           /*tp_compare*/
    0,                           /*tp_repr*/
    0,                           /*tp_as_number*/
    0,                           /*tp_as_sequence*/
    0,                           /*tp_as_mapping*/
    0,                           /*tp_hash */
    0,                           /*tp_call*/
    0,                           /*tp_str*/
    0,                           /*tp_getattro*/
    0,                           /*tp_setattro*/
    0,                           /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,          /*tp_flags*/
    "A MongoDB Database.",       /*tp_doc*/
};


PyObject *
pymongoc_database_new (mongoc_database_t *database)
{
   pymongoc_database_t *pydatabase;

   BSON_ASSERT (database);

   pydatabase =
      (pymongoc_database_t *)PyType_GenericNew (&pymongoc_database_type,
                                                NULL, NULL);
   if (!pydatabase) {
      return NULL;
   }

   pydatabase->database = database;

   return (PyObject *)pydatabase;
}


PyTypeObject *
pymongoc_database_get_type (void)
{
   static bool initialized;

   if (!initialized) {
      if (PyType_Ready (&pymongoc_database_type) < 0) {
         return NULL;
      }
      initialized = true;
   }

   return &pymongoc_database_type;
}
