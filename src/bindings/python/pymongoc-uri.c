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


#include <string.h>

#include "pymongoc-uri.h"


static void
pymongoc_uri_tp_dealloc (PyObject *self)
{
   pymongoc_uri_t *uri = (pymongoc_uri_t *)self;
   mongoc_uri_destroy (uri->uri);
   self->ob_type->tp_free (self);
}


static PyObject *
pymongoc_uri_tp_repr (PyObject *obj)
{
   pymongoc_uri_t *uri = (pymongoc_uri_t *)obj;
   PyObject *ret;
   const char *str;
   char *repr;

   str = mongoc_uri_get_string (uri->uri);
   repr = bson_strdup_printf ("URI(\"%s\")", str);
   ret = PyString_FromStringAndSize (repr, strlen (repr));
   bson_free (repr);
   return ret;
}


static PyObject *
pymongoc_uri_tp_str (PyObject *obj)
{
   pymongoc_uri_t *uri = (pymongoc_uri_t *)obj;
   PyObject *ret;
   const char *str;

   str = mongoc_uri_get_string (uri->uri);
   ret = PyString_FromStringAndSize (str, strlen (str));

   return ret;
}


static PyTypeObject pymongoc_uri_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "pymongc.URI",             /*tp_name*/
    sizeof(pymongoc_uri_t),    /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    pymongoc_uri_tp_dealloc,   /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    pymongoc_uri_tp_repr,      /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    pymongoc_uri_tp_str,       /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "A MongoDB URI.",          /*tp_doc*/
};


static PyObject *
pymongoc_uri_tp_new (PyTypeObject *self,
                     PyObject     *args,
                     PyObject     *kwargs)
{
   pymongoc_uri_t *pyuri;
   const char *uri_str = NULL;

   if (!PyArg_ParseTuple (args, "s", &uri_str)) {
      return NULL;
   }

   pyuri = (pymongoc_uri_t *)PyType_GenericNew (&pymongoc_uri_type,
                                                NULL, NULL);

   if (!pyuri) {
      return NULL;
   }

   pyuri->uri = mongoc_uri_new (uri_str);

   if (!pyuri->uri) {
      PyErr_SetString (PyExc_TypeError, "Invalid URI string.");
      Py_DECREF (pyuri);
      return NULL;
   }

   return (PyObject *)pyuri;
}


PyTypeObject *
pymongoc_uri_get_type (void)
{
   static bool initialized;

   if (!initialized) {
      pymongoc_uri_type.tp_new = pymongoc_uri_tp_new;
      if (PyType_Ready(&pymongoc_uri_type) < 0) {
         return NULL;
      }
      initialized = true;
   }

   return &pymongoc_uri_type;
}
