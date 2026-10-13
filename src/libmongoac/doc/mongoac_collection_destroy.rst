:man_page: mongoac_collection_destroy

mongoac_collection_destroy()
============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   void mongoac_collection_destroy(mongoac_collection_t *coll);

Description
-----------

Destroy the given :symbol:`mongoac_collection_t` and decrement its shared state's reference counter.

When ``coll`` is null, does nothing.

.. note::

   The shared state is refcounted: it is deallocated only when its reference count is reduced to zero.

.. seealso::

   | :symbol:`mongoac_collection_clone`
   | :symbol:`mongoac_database_get_collection`
