:man_page: mongoc_bulk_operation_get_database

mongoc_bulk_operation_get_database()
===================================

Synopsis
--------

.. code-block:: c

  const char*
  mongoc_bulk_operation_get_database (mongoc_bulk_operation_t *bulk);

Parameters
----------

* ``bulk``: A :symbol:`mongoc_bulk_operation_t`.

Description
-----------

Retrieves the database name.

Returns
-------

A C string that should not be modified or freed.
