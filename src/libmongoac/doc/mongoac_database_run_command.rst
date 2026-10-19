:man_page: mongoac_database_run_command

mongoac_database_run_command()
==============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_bson_t mongoac_database_run_command(const mongoac_database_t *db,
                                               mongoac_client_session_t *session,
                                               mongoac_bson_view_t command,
                                               const mongoac_run_command_options_t *options,
                                               mongoac_error_t *error);

Description
-----------

Run the given ``command`` via `Database::run_raw_command() <https://docs.rs/mongodb/latest/mongodb/struct.Database.html#method.run_raw_command>`_.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``db`` is null.
- Invalid Argument (mongoac): ``command`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.
- Client-Side Error (bson): the server reply contained invalid BSON.

.. seealso::

   | :symbol:`mongoac_database_run_command_async()`
