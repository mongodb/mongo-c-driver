:man_page: mongoac_client_get_runtime

mongoac_client_get_runtime()
============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   mongoac_runtime_t *mongoac_client_get_runtime(const mongoac_client_t *client);

Description
-----------

Return the :symbol:`mongoac_runtime_t` associated with the given :symbol:`mongoac_client_t`.

When ``client`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_runtime_destroy()`.

.. seealso::

   | :symbol:`mongoac_runtime_clone`
