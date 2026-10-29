:man_page: mongoac_collection_insert_many_async

mongoac_collection_insert_many_async()
======================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_future_t *mongoac_collection_insert_many_async(const mongoac_collection_t *collection,
                                                          mongoac_client_session_t *session,
                                                          const mongoac_bson_view_t *docs,
                                                          uintptr_t count,
                                                          const mongoac_insert_many_options_t *options);

Description
-----------

Async equivalent to :symbol:`mongoac_collection_insert_many()`.

When ``collection`` is null, returns ``NULL``.

When ``docs`` is null or empty, returns ``NULL``.

When a document in ``docs`` is null or invalid BSON, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. seealso::

   | :symbol:`mongoac_future_t`
