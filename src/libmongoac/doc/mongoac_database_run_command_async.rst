:man_page: mongoac_database_run_command_async

mongoac_database_run_command_async()
====================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_future_t *mongoac_database_run_command_async(const mongoac_database_t *db,
                                                        mongoac_client_session_t *session,
                                                        mongoac_bson_view_t command,
                                                        const mongoac_run_command_options_t *options,
                                                        mongoac_error_t *error);

Description
-----------

Async equivalent to :symbol:`mongoac_database_run_command()`.

When ``db`` is null, returns ``NULL``.

When ``command`` is null or invalid BSON, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. seealso::

   | :symbol:`mongoac_future_t`
