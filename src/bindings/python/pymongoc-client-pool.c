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
#include "pymongoc-client-pool.h"


static void
pymongoc_client_pool_tp_dealloc (PyObject *self)
{
   pymongoc_client_pool_t *pool = (pymongoc_client_pool_t *)self;

   ENTRY;

   mongoc_client_pool_destroy (pool->client_pool);

   self->ob_type->tp_free (self);

   EXIT;
}


static PyTypeObject pymongoc_client_pool_type = {
    PyObject_HEAD_INIT(NULL)
    0,                               /*ob_size*/
    "pymongc.ClientPool",            /*tp_name*/
    sizeof(pymongoc_client_pool_t),  /*tp_basicsize*/
    0,                               /*tp_itemsize*/
    pymongoc_client_pool_tp_dealloc, /*tp_dealloc*/
    0,                               /*tp_print*/
    0,                               /*tp_getattr*/
    0,                               /*tp_setattr*/
    0,                               /*tp_compare*/
    0,                               /*tp_repr*/
    0,                               /*tp_as_number*/
    0,                               /*tp_as_sequence*/
    0,                               /*tp_as_mapping*/
    0,                               /*tp_hash */
    0,                               /*tp_call*/
    0,                               /*tp_str*/
    0,                               /*tp_getattro*/
    0,                               /*tp_setattro*/
    0,                               /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,              /*tp_flags*/
    "A MongoDB Client Pool.",        /*tp_doc*/
};


static PyObject *
pymongoc_client_pool_tp_new (PyTypeObject *self,
                             PyObject     *args,
                             PyObject     *kwargs)
{
   pymongoc_client_pool_t *pyclient_pool;
   mongoc_uri_t *uri;
   const char *uri_str;
   PyObject *key = NULL;
   PyObject *pyuri = NULL;
   PyObject *ret = NULL;

   if (kwargs) {
      key = PyString_FromStringAndSize("uri", 3);
      if (PyDict_Contains(kwargs, key)) {
         if (!(pyuri = PyDict_GetItem(kwargs, key))) {
            goto cleanup;
         } else if (!PyString_Check(pyuri)) {
            PyErr_SetString(PyExc_TypeError, "uri must be a string.");
            goto cleanup;
         }
      }
   }

   uri_str = pyuri ? PyString_AsString(pyuri) : NULL;
   uri = mongoc_uri_new (uri_str);

   pyclient_pool = (pymongoc_client_pool_t *)
      PyType_GenericNew (&pymongoc_client_pool_type, NULL, NULL);
   if (!pyclient_pool) {
      goto cleanup;
   }

   pyclient_pool->client_pool = mongoc_client_pool_new (uri);
   if (!pyclient_pool->client_pool) {
      PyErr_SetString (PyExc_TypeError, "Invalid URI string.");
      Py_DECREF (pyclient_pool);
      pyclient_pool = NULL;
      goto cleanup;
   }

   ret = (PyObject *)pyclient_pool;

cleanup:
   if (uri) {
      mongoc_uri_destroy (uri);
   }
   Py_XDECREF (key);
   Py_XDECREF (pyuri);

   return ret;
}


static PyObject *
pymongoc_client_pool_pop (PyObject *self,
                          PyObject *args)
{
   pymongoc_client_pool_t *client_pool = (pymongoc_client_pool_t *)self;
   mongoc_client_t *client;

   if (!pymongoc_client_pool_check (self)) {
      PyErr_SetString (PyExc_TypeError,
                       "self must be a pymongoc.ClientPool");
      return NULL;
   }

   BSON_ASSERT (client_pool->client_pool);

   client = mongoc_client_pool_pop (client_pool->client_pool);

   return pymongoc_client_new (client, false);
}


static PyObject *
pymongoc_client_pool_push (PyObject *self,
                           PyObject *args)
{
   pymongoc_client_pool_t *client_pool = (pymongoc_client_pool_t *)self;
   pymongoc_client_t *pyclient = NULL;

   if (!PyArg_ParseTuple (args, "O", &pyclient)) {
      return NULL;
   }

   if (!pymongoc_client_check (pyclient)) {
      PyErr_SetString (PyExc_TypeError,
                       "pymongoc.ClientPool.push only accepts a "
                       "pymongoc.Client.");
      return NULL;
   }

   if (pyclient->client) {
      mongoc_client_pool_push (client_pool->client_pool,
                               pyclient->client);
      pyclient->client = NULL;
   }

   Py_INCREF (Py_None);

   return Py_None;
}


static PyMethodDef pymongoc_client_pool_methods[] = {
   { "pop", pymongoc_client_pool_pop, METH_VARARGS,
     "Pop a pymongoc.Client from the client pool, possibly blocking "
     "until one is available." },
   { "push", pymongoc_client_pool_push, METH_VARARGS,
     "Return a pymongoc.Client to the client pool, possibly allowing "
     "another thread to steal the client." },
   { NULL }
};


PyTypeObject *
pymongoc_client_pool_get_type (void)
{
   static bool initialized;

   if (!initialized) {
      pymongoc_client_pool_type.tp_new = pymongoc_client_pool_tp_new;
      pymongoc_client_pool_type.tp_methods = pymongoc_client_pool_methods;
      if (PyType_Ready (&pymongoc_client_pool_type) < 0) {
         return NULL;
      }
      initialized = true;
   }

   return &pymongoc_client_pool_type;
}
