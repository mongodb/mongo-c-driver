:man_page: mongoac_database_create_collection

mongoac_database_create_collection()
====================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   void mongoac_database_create_collection(mongoac_database_t *db,
                                           mongoac_client_session_t *session,
                                           mongoac_string_view_t name,
                                           const mongoac_create_collection_options_t *options,
                                           mongoac_error_t *error);

Description
-----------

Creates a new collection with the specified ``name`` in the database, via `Database::create_collection() <https://docs.rs/mongodb/latest/mongodb/struct.Database.html#method.create_collection>`_.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``db`` is null.
- Invalid Argument (mongoac): ``name`` is null or invalid UTF-8.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_database_create_collection_async()`
