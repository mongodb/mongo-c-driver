:man_page: mongoac_client_get_runtime_address

mongoac_client_get_runtime_address()
====================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   uintptr_t mongoac_client_get_runtime_address(const mongoac_client_t *client);

Description
-----------

Returns the unique address of the shared state of the :symbol:`mongoac_runtime_t` associated with the given :symbol:`mongoac_client_t`.

When ``client`` is null, returns ``0``.

.. important::

   The address may only be equality-compared with other addresses returned by other ``*_runtime_address()`` functions.

.. seealso::

   | :symbol:`mongoac_client_get_runtime`
