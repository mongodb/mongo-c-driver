:man_page: mongoac_client_shutdown

mongoac_client_shutdown()
=========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   void mongoac_client_shutdown(mongoac_client_t *client);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

Description
-----------

Terminate background tasks and close connections associated with the given :symbol:`mongoac_client_t`.

When ``client`` is null, does nothing.

Any attempted operation on the given client after it is shutdown will return an error.

.. warning::

   Ensure all objects associated with the given client (excluding the associated runtime) are destroyed prior to calling this function.
   Otherwise, this function may `block indefinitely <https://docs.rs/mongodb/latest/mongodb/struct.Client.html#method.shutdown>`_.

.. important::

   Calling :symbol:`mongoac_client_shutdown()` before :symbol:`mongoac_client_destroy()` is highly recommended to ensure a `clean shutdown <https://docs.rs/mongodb/latest/mongodb/struct.Client.html#clean-shutdown>`_.

.. seealso::

   | :symbol:`mongoac_client_destroy`
   | :symbol:`mongoac_error_t`
