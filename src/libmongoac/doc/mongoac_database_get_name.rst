:man_page: mongoac_database_get_name

mongoac_database_get_name()
===========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database.h>

   mongoac_string_view_t mongoac_database_get_name(const mongoac_database_t *db);

Description
-----------

Returns the name of the given :symbol:`mongoac_database_t`.

When ``db`` is null, returns a null string.

.. important::

   The returned string is owned by the database object and must not be modified or freed.
