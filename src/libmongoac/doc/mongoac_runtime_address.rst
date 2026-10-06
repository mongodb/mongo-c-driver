:man_page: mongoac_runtime_address

mongoac_runtime_address()
=========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/runtime.h>

   uintptr_t mongoac_runtime_address(const mongoac_runtime_t *runtime);

Description
-----------

Returns the unique address of the shared state for the given :symbol:`mongoac_runtime_t`.

When ``runtime`` is null, returns ``0``.

.. important::

   The address may only be equality-compared with other addresses returned by :symbol:`mongoac_runtime_address()`.

.. seealso::

   | :symbol:`mongoac_runtime_clone`
   | :symbol:`mongoac_runtime_t`
