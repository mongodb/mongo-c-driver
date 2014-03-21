/*
 * Copyright 2013 MongoDB, Inc.
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
#include "pymongoc-client.h"


static void
pymongoc_client_tp_dealloc (PyObject *self)
{
   pymongoc_client_t *client = (pymongoc_client_t *)self;

   ENTRY;

   if (client->owns_client && client->client) {
      mongoc_client_destroy (client->client);
   }

   self->ob_type->tp_free(self);

   EXIT;
}


static PyTypeObject pymongoc_client_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "pymongc.Client",          /*tp_name*/
    sizeof(pymongoc_client_t), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    pymongoc_client_tp_dealloc,/*tp_dealloc*/
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
    "A MongoDB Client.",       /*tp_doc*/
};


PyObject *
pymongoc_client_new (mongoc_client_t *client,
                     bool             owns_client)
{
   pymongoc_client_t *pyclient;

   BSON_ASSERT (client);

   pyclient = (pymongoc_client_t *)
      PyType_GenericNew (&pymongoc_client_type, NULL, NULL);
   if (!pyclient) {
      return NULL;
   }

   pyclient->client = client;
   pyclient->owns_client = owns_client;

   return (PyObject *)pyclient;
}


static PyObject *
pymongoc_client_tp_new (PyTypeObject *self,
                        PyObject     *args,
                        PyObject     *kwargs)
{
   mongoc_client_t *client;
   const char *uri_str;
   PyObject *key = NULL;
   PyObject *uri = NULL;
   PyObject *ret = NULL;

   if (kwargs) {
      key = PyString_FromStringAndSize("uri", 3);
      if (PyDict_Contains(kwargs, key)) {
         if (!(uri = PyDict_GetItem(kwargs, key))) {
            goto cleanup;
         } else if (!PyString_Check(uri)) {
            PyErr_SetString(PyExc_TypeError, "uri must be a string.");
            goto cleanup;
         }
      }
   }

   uri_str = uri ? PyString_AsString(uri) : NULL;

   if (!(client = mongoc_client_new (uri_str))) {
      PyErr_SetString (PyExc_ValueError,
                       "Failed to create client, check URI.");
      goto cleanup;
   }

   ret = pymongoc_client_new (client, true);

cleanup:
   Py_XDECREF(key);
   Py_XDECREF(uri);

   return ret;
}


PyTypeObject *
pymongoc_client_get_type (void)
{
   static bool initialized;

   if (!initialized) {
      pymongoc_client_type.tp_new = pymongoc_client_tp_new;
      if (PyType_Ready(&pymongoc_client_type) < 0) {
         return NULL;
      }
      initialized = true;
   }

   return &pymongoc_client_type;
}
