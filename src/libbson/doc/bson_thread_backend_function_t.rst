:man_page: bson_thread_backend_function_t

bson_thread_backend_function_t
==============================

The function type accepted by the ``thread_create`` member of
:symbol:`bson_thread_backend_t`.

.. code-block:: c

  #ifdef _WIN32
  typedef unsigned (__stdcall *bson_thread_backend_function_t) (void *);
  #else
  typedef void *(*bson_thread_backend_function_t) (void *);
  #endif

Call the function with the argument provided to ``thread_create``. Its return
value is ignored by the driver. A successful ``thread_join`` must wait until
this function returns.
