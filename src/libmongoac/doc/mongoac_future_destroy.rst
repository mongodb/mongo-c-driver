:man_page: mongoac_future_destroy

mongoac_future_destroy()
========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/future.h>

   void mongoac_future_destroy(mongoac_future_t *future);

Description
-----------

Destroy the given :symbol:`mongoac_future_t` and decrement its shared state's reference counter.

When ``future`` is null, does nothing.

.. attention::

   This function does not :ref:`cancel <cancellation>` the async operation when it is still pending.

.. note::

   The shared state is refcounted: it is deallocated only when its reference count is reduced to zero.

.. seealso::

   | :symbol:`mongoac_future_clone`
   | :symbol:`mongoac_future_t`
