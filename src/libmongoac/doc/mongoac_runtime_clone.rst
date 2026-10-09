:man_page: mongoac_runtime_clone

mongoac_runtime_clone()
=======================

Synopsis
--------

.. code-block:: c

   #include <mongoac/runtime.h>

   mongoac_runtime_t *mongoac_runtime_clone(const mongoac_runtime_t *runtime);

Description
-----------

Returns a new :symbol:`mongoac_runtime_t` with the same shared state as ``runtime``.

When ``runtime`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_runtime_destroy()`.

.. note::

   The shared state is refcounted: cloning is cheap and increments the reference count.

.. seealso::

   | :symbol:`mongoac_client_get_runtime`
   | :symbol:`mongoac_runtime_destroy`
