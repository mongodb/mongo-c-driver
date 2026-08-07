:man_page: mongoc_bulk_operation_get_let

mongoc_bulk_operation_get_let()
===============================

Synopsis
--------

.. code-block:: c

  const bson_t*
  mongoc_bulk_operation_get_let (mongoc_bulk_operation_t *bulk);

Parameters
----------

* ``bulk``: A :symbol:`mongoc_bulk_operation_t`.

Description
-----------

Retrieves a BSON document consisting of any number of parameter names, each followed by definitions of constants in the MQL Aggregate Expression language.

These constants can be accessed by all update, replace, and delete operations executed as part of this :doc:`bulk <mongoc_bulk_operation_t>`.
