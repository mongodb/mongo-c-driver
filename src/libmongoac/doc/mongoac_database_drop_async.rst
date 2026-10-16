:man_page: mongoac_database_drop_async

mongoac_database_drop_async()
=============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_future_t *mongoac_database_drop_async(const mongoac_database_t *db,
                                                 mongoac_client_session_t *session,
                                                 const mongoac_drop_database_options_t *options);

Description
-----------

Async equivalent to :symbol:`mongoac_database_drop()`.

When ``db`` is null, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. seealso::

   | :symbol:`mongoac_future_t`
