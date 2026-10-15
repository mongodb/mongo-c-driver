:man_page: mongoac_client_destroy

mongoac_client_destroy()
========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   void mongoac_client_destroy(mongoac_client_t *client);

Description
-----------

Destroy the given :symbol:`mongoac_client_t` and decrement its shared state's reference counter.

When ``client`` is null, does nothing.

.. important::

   Calling :symbol:`mongoac_client_shutdown()` before :symbol:`mongoac_client_destroy()` is highly recommended to ensure a `clean shutdown <https://docs.rs/mongodb/latest/mongodb/struct.Client.html#clean-shutdown>`_.

.. note::

   The shared state is refcounted: it is deallocated only when its reference count is reduced to zero.

.. seealso::

   | :symbol:`mongoac_client_clone`
   | :symbol:`mongoac_client_new`
   | :symbol:`mongoac_client_shutdown`
