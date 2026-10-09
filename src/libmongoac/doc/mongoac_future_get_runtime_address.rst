:man_page: mongoac_future_get_runtime_address

mongoac_future_get_runtime_address()
====================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/future.h>

   uintptr_t mongoac_future_get_runtime_address(const mongoac_future_t *future);

Description
-----------

Returns the unique address of the shared state of the :symbol:`mongoac_runtime_t` associated with the given :symbol:`mongoac_future_t`.

When ``future`` is null, returns ``0``.

.. important::

   The address may only be equality-compared with other addresses returned by other ``*_runtime_address()`` functions.

.. seealso::

   | :symbol:`mongoac_future_get_runtime`
   | :symbol:`mongoac_runtime_address`
