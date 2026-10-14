:man_page: mongoac_database_get_runtime_address

mongoac_database_get_runtime_address()
======================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   uintptr_t mongoac_database_get_runtime_address(const mongoac_database_t *db);

Description
-----------

Returns the unique address of the shared state of the :symbol:`mongoac_runtime_t` associated with the given :symbol:`mongoac_database_t`.

When ``db`` is null, returns ``0``.

.. important::

   The address may only be equality-compared with other addresses returned by other ``*_runtime_address()`` functions.

.. seealso::

   | :symbol:`mongoac_database_get_runtime`
