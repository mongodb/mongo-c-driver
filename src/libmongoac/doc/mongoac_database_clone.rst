:man_page: mongoac_database_clone

mongoac_database_clone()
========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_database_t *mongoac_database_clone(const mongoac_database_t *db);

Description
-----------

Returns a new :symbol:`mongoac_database_t` with the same shared state as ``db``.

When ``db`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_database_destroy()`.

.. note::

   The shared state is refcounted: cloning is cheap and increments the reference count.

.. seealso::

   | :symbol:`mongoac_database_destroy`
   | :symbol:`mongoac_client_get_database`
