:man_page: mongoac_collection_count_documents_async

mongoac_collection_count_documents_async()
==========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_future_t *mongoac_collection_count_documents_async(const mongoac_collection_t *coll,
                                                              mongoac_client_session_t *session,
                                                              mongoac_bson_view_t filter,
                                                              const mongoac_count_options_t *options,
                                                              mongoac_error_t *error);

Description
-----------

Async equivalent to :symbol:`mongoac_collection_count_documents()`.

When ``coll`` is null, returns ``NULL``.

When ``filter`` is null or invalid BSON, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`. Use :symbol:`mongoac_future_get_uint64()` to obtain the resulting count.

.. seealso::

   | :symbol:`mongoac_future_t`
