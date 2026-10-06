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

.. seealso::

   | :symbol:`mongoac_client_destroy`
   | :symbol:`mongoac_error_t`
