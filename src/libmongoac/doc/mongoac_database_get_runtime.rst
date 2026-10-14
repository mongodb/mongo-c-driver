:man_page: mongoac_database_get_runtime

mongoac_database_get_runtime()
==============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_runtime_t *mongoac_database_get_runtime(const mongoac_database_t *db);

Description
-----------

Returns the :symbol:`mongoac_runtime_t` associated with the given :symbol:`mongoac_database_t`.

When ``db`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_runtime_destroy()`.

.. seealso::

   | :symbol:`mongoac_database_get_runtime_address`
