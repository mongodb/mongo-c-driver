:man_page: mongoac_database_get_collection

mongoac_database_get_collection()
=================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_collection_t *mongoac_database_get_collection(const mongoac_database_t *db,
                                                         mongoac_string_view_t name,
                                                         const mongoac_collection_options_t *options);

Description
-----------

Returns a new :symbol:`mongoac_collection_t` with the specified ``name`` and ``options``.

When ``db`` is null, returns a null pointer.

When ``name`` is null or invalid UTF-8, returns a null pointer.

Must be destroyed with :symbol:`mongoac_collection_destroy()`.

.. seealso::

   | :symbol:`mongoac_collection_clone`
