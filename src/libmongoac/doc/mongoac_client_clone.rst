:man_page: mongoac_client_clone

mongoac_client_clone()
======================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   mongoac_client_t *mongoac_client_clone(const mongoac_client_t *client);

Description
-----------

Returns a new :symbol:`mongoac_client_t` with the same shared state as ``client``.

When ``client`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_client_destroy()`.

.. note::

   The shared state is refcounted: cloning is cheap and increments the reference count.

.. seealso::

   | :symbol:`mongoac_client_destroy`
   | :symbol:`mongoac_client_new`
