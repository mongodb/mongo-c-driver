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


#include "pymongoc-client.h"
#include "pymongoc-client-pool.h"
#include "pymongoc-uri.h"


static PyMethodDef pymongoc_methods[] = {
   { NULL }
};


PyMODINIT_FUNC
initpymongoc (void)
{
   PyObject *module;
   PyObject *version;

   /*
    * Initialize our module with class methods.
    */
   if (!(module = Py_InitModule("pymongoc", pymongoc_methods))) {
      return;
   }

   /*
    * Register the library version as __version__.
    */
   version = PyString_FromString(MONGOC_VERSION_S);
   PyModule_AddObject(module, "__version__", version);

   /*
    * Register cmongo types.
    */
   PyModule_AddObject (module, "Client", (PyObject *)pymongoc_client_get_type ());
   PyModule_AddObject (module, "ClientPool", (PyObject *)pymongoc_client_pool_get_type ());
   PyModule_AddObject (module, "URI", (PyObject *)pymongoc_uri_get_type ());
}
