:man_page: mongoac_collection_get_runtime_address

mongoac_collection_get_runtime_address()
========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   uintptr_t mongoac_collection_get_runtime_address(const mongoac_collection_t *coll);

Description
-----------

Returns the unique address of the shared state of the :symbol:`mongoac_runtime_t` associated with the given :symbol:`mongoac_collection_t`.

When ``coll`` is null, returns ``0``.

.. important::

   The address may only be equality-compared with other addresses returned by other ``*_runtime_address()`` functions.

.. seealso::

   | :symbol:`mongoac_collection_get_runtime`
