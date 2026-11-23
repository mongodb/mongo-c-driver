:man_page: mongoac_collection_estimated_document_count_async

mongoac_collection_estimated_document_count_async()
===================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_future_t *mongoac_collection_estimated_document_count_async(const mongoac_collection_t *coll,
                                                                       const mongoac_estimated_document_count_options_t *options);

Description
-----------

Async equivalent to :symbol:`mongoac_collection_estimated_document_count()`.

When ``coll`` is null, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. seealso::

   | :symbol:`mongoac_future_t`
