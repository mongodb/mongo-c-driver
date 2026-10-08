:man_page: mongoac_client_shutdown_async

mongoac_client_shutdown_async()
===============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   mongoac_future_t *mongoac_client_shutdown_async(mongoac_client_t *client);

Description
-----------

Async equivalent to :symbol:`mongoac_client_shutdown()`.

When ``client`` is null, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. seealso::

   | :symbol:`mongoac_client_destroy`
   | :symbol:`mongoac_future_t`
