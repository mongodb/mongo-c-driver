:man_page: mongoac_collection_get_client

mongoac_collection_get_client()
===============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_client_t *mongoac_collection_get_client(const mongoac_collection_t *coll);

Description
-----------

Returns a new :symbol:`mongoac_client_t` with the same shared state as the client associated with :symbol:`mongoac_collection_t`.

When ``coll`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_client_destroy()`.

.. seealso::

   | :symbol:`mongoac_client_clone`
