:man_page: mongoac_client_get_database

mongoac_client_get_database()
=============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   mongoac_database_t *mongoac_client_get_database(const mongoac_client_t *client,
                                                   mongoac_string_view_t name,
                                                   const mongoac_database_options_t *options);

Description
-----------

Returns a new :symbol:`mongoac_database_t` with the specified ``name`` and ``options``.

When ``client`` is null, returns a null pointer.

When ``name`` is null or invalid UTF-8, returns a null pointer.

Must be destroyed with :symbol:`mongoac_database_destroy()`.

.. seealso::

   | :symbol:`mongoac_database_clone`
