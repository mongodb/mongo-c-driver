:man_page: mongoac_future_get_runtime

mongoac_future_get_runtime()
============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/future.h>

   mongoac_runtime_t *mongoac_future_get_runtime(const mongoac_future_t *future);

Description
-----------

Return a :symbol:`mongoac_runtime_t` with the same shared state as the runtime associated with the given :symbol:`mongoac_future_t`.

Must be destroyed with :symbol:`mongoac_runtime_destroy()`.

When ``future`` is null, returns a null pointer.

.. note::

   The shared state is refcounted: cloning is cheap and increments the reference count.

.. seealso::

   | :symbol:`mongoac_future_get_runtime_address`
   | :symbol:`mongoac_runtime_t`
