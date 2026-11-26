:man_page: mongoac_database_aggregate

mongoac_database_aggregate()
============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_cursor_t *mongoac_database_aggregate(const mongoac_database_t *db,
                                                mongoac_client_session_t *session,
                                                mongoac_bson_view_array_t pipeline,
                                                const mongoac_aggregate_options_t *options,
                                                mongoac_error_t *error);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

Description
-----------

Runs the given ``pipeline`` via `Database::aggregate() <https://docs.rs/mongodb/latest/mongodb/struct.Database.html#method.aggregate>`_.

The ``options`` parameter must be ``NULL``.

The returned :symbol:`mongoac_cursor_t` must be destroyed with :symbol:`mongoac_cursor_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``db`` is null.
- Invalid Argument (mongoac): ``pipeline`` is null.
- Invalid Argument (mongoac): a stage in ``pipeline`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_database_aggregate_async()`
   | :symbol:`mongoac_cursor_t`
