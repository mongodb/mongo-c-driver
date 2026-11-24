:man_page: mongoac_collection_aggregate_async

mongoac_collection_aggregate_async()
====================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_future_t *mongoac_collection_aggregate_async(const mongoac_collection_t *collection,
                                                        mongoac_client_session_t *session,
                                                        mongoac_bson_view_array_t pipeline,
                                                        const mongoac_aggregate_options_t *options);

Description
-----------

Async equivalent to :symbol:`mongoac_collection_aggregate()`.

When ``collection`` is null, returns ``NULL``.

When ``pipeline`` is null, or a stage in ``pipeline`` is null or invalid BSON, returns ``NULL``.

The ``options`` parameter must be ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. seealso::

   | :symbol:`mongoac_future_get_cursor`
   | :symbol:`mongoac_cursor_t`
