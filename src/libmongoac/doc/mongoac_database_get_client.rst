:man_page: mongoac_database_get_client

mongoac_database_get_client()
=============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_client_t *mongoac_database_get_client(const mongoac_database_t *db);

Description
-----------

Returns a new :symbol:`mongoac_client_t` with the same shared state as the client associated with :symbol:`mongoac_database_t`.

When ``db`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_client_destroy()`.

.. seealso::

   | :symbol:`mongoac_client_clone`
