:man_page: mongoac_collection_update_many_document_async

mongoac_collection_update_many_document_async()
===============================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_future_t *mongoac_collection_update_many_document_async(const mongoac_collection_t *collection,
                                                                   mongoac_client_session_t *session,
                                                                   mongoac_bson_view_t filter,
                                                                   mongoac_bson_view_t update,
                                                                   const mongoac_update_options_t *options);

Description
-----------

Async equivalent to :symbol:`mongoac_collection_update_many_document()`.

When ``collection`` is null, returns ``NULL``.

When ``filter`` is null or invalid BSON, returns ``NULL``.

When ``update`` is null or invalid BSON, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. seealso::

   | :symbol:`mongoac_future_t`
