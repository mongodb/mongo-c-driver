:man_page: mongoac_collection_get_runtime

mongoac_collection_get_runtime()
================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_runtime_t *mongoac_collection_get_runtime(const mongoac_collection_t *coll);

Description
-----------

Returns the :symbol:`mongoac_runtime_t` associated with the given :symbol:`mongoac_collection_t`.

When ``coll`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_runtime_destroy()`.

.. seealso::

   | :symbol:`mongoac_runtime_clone`
