:man_page: mongoac_database_run_cursor_command

mongoac_database_run_cursor_command()
=====================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_cursor_t *mongoac_database_run_cursor_command(const mongoac_database_t *db,
                                                         mongoac_client_session_t *session,
                                                         mongoac_bson_view_t command,
                                                         const mongoac_run_cursor_command_options_t *options,
                                                         mongoac_error_t *error);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

Description
-----------

Run the given ``command`` via `Database::run_raw_cursor_command() <https://docs.rs/mongodb/latest/mongodb/struct.Database.html#method.run_raw_cursor_command>`_.

The returned :symbol:`mongoac_cursor_t` must be destroyed with :symbol:`mongoac_cursor_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``db`` is null.
- Invalid Argument (mongoac): ``command`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_database_run_cursor_command_async()`
   | :symbol:`mongoac_cursor_t`
