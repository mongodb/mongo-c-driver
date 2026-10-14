:man_page: mongoac_database_drop

mongoac_database_drop()
=======================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   void mongoac_database_drop(const mongoac_database_t *db,
                              mongoac_client_session_t *session,
                              const mongoac_drop_database_options_t *options,
                              mongoac_error_t *error);

Description
-----------

Drops the database via `Database::drop() <https://docs.rs/mongodb/latest/mongodb/struct.Database.html#method.drop>`_.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``db`` is null.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_database_drop_async()`
