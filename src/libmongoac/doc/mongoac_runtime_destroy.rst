:man_page: mongoac_runtime_destroy

mongoac_runtime_destroy()
=========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/runtime.h>

   void mongoac_runtime_destroy(mongoac_runtime_t *runtime);

Description
-----------

Destroys the given :symbol:`mongoac_runtime_t` and decrements its shared state's reference counter.

When ``runtime`` is null, does nothing.

.. important::

   Any incomplete tasks spawned with the given runtime are `dropped <https://docs.rs/tokio/latest/tokio/runtime/struct.Runtime.html#shutdown>`_ and are not guaranteed to run to completion.

.. note::

   The shared state is refcounted: it is deallocated only when its reference count is reduced to zero.

.. seealso::

   | :symbol:`mongoac_client_get_runtime`
   | :symbol:`mongoac_runtime_clone`
