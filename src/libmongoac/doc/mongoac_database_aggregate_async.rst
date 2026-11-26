:man_page: mongoac_database_aggregate_async

mongoac_database_aggregate_async()
==================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_future_t *mongoac_database_aggregate_async(const mongoac_database_t *db,
                                                      mongoac_client_session_t *session,
                                                      mongoac_bson_view_array_t pipeline,
                                                      const mongoac_aggregate_options_t *options);

Description
-----------

Async equivalent to :symbol:`mongoac_database_aggregate()`.

When ``db`` is null, returns ``NULL``.

When ``pipeline`` is null, or a stage in ``pipeline`` is null or invalid BSON, returns ``NULL``.

The ``options`` parameter must be ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. seealso::

   | :symbol:`mongoac_future_get_cursor`
   | :symbol:`mongoac_cursor_t`
