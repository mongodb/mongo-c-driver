:man_page: mongoac_database_destroy

mongoac_database_destroy()
==========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   void mongoac_database_destroy(mongoac_database_t *db);

Description
-----------

Destroy the given :symbol:`mongoac_database_t` and decrement its shared state's reference counter.

When ``db`` is null, does nothing.

.. note::

   The shared state is refcounted: it is deallocated only when its reference count is reduced to zero.

.. seealso::

   | :symbol:`mongoac_database_clone`
   | :symbol:`mongoac_client_get_database`
