:man_page: mongoac_client_get_default_database

mongoac_client_get_default_database()
=====================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client.h>

   mongoac_database_t *mongoac_client_get_default_database(const mongoac_client_t *client);

Description
-----------

Returns a :symbol:`mongoac_database_t` for the default database specified in the ``ClientOptions`` or MongoDB connection string used to construct this ``Client``, via `Client::default_database() <https://docs.rs/mongodb/latest/mongodb/struct.Client.html#method.default_database>`_.

When ``client`` is null, returns a null pointer.

When no default database is specified, returns a null pointer.

Must be destroyed with :symbol:`mongoac_database_destroy()`.

.. seealso::

   | :symbol:`mongoac_client_get_database()`
