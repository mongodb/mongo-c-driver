:man_page: mongoac_database_create_collection_async

mongoac_database_create_collection_async()
==========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_future_t *mongoac_database_create_collection_async(mongoac_database_t *db,
                                                              mongoac_client_session_t *session,
                                                              mongoac_string_view_t name,
                                                              const mongoac_create_collection_options_t *options);

Description
-----------

Async equivalent to :symbol:`mongoac_database_create_collection()`.

When ``db`` is null, returns ``NULL``.

When ``name`` is null or invalid UTF-8, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. seealso::

   | :symbol:`mongoac_future_t`
