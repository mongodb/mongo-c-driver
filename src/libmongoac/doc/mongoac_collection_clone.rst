:man_page: mongoac_collection_clone

mongoac_collection_clone()
==========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_collection_t *mongoac_collection_clone(const mongoac_collection_t *coll);

Description
-----------

Returns a new :symbol:`mongoac_collection_t` with the same shared state as ``coll``.

When ``coll`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_collection_destroy()`.

.. note::

   The shared state is refcounted: cloning is cheap and increments the reference count.

.. seealso::

   | :symbol:`mongoac_collection_destroy`
   | :symbol:`mongoac_database_get_collection`
