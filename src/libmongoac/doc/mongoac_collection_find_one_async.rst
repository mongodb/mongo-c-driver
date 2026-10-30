:man_page: mongoac_collection_find_one_async

mongoac_collection_find_one_async()
===================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_future_t *mongoac_collection_find_one_async(const mongoac_collection_t *collection,
                                                       mongoac_client_session_t *session,
                                                       mongoac_bson_view_t filter,
                                                       const mongoac_find_one_options_t *options);

Description
-----------

Async equivalent to :symbol:`mongoac_collection_find_one()`.

When ``collection`` is null, returns ``NULL``.

When ``filter`` is null or invalid BSON, returns ``NULL``.

The ``options`` parameter must be ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. note::

   The result of the async operation is obtained with :symbol:`mongoac_future_get_optional_bson()`, not :symbol:`mongoac_future_get_bson()`.

.. seealso::

   | :symbol:`mongoac_future_t`
   | :symbol:`mongoac_future_get_optional_bson()`
