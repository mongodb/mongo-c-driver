:man_page: mongoc_bulk_operation_insert

mongoc_bulk_operation_insert()
==============================

Synopsis
--------

.. code-block:: none

  void
  mongoc_bulk_operation_insert (mongoc_bulk_operation_t *bulk,
                                const bson_t            *document);

Queue an insert of a single document into a bulk operation. The insert is not performed until :symbol:`mongoc_bulk_operation_execute() <mongoc_bulk_operation_execute>` is called.

Parameters
----------

* ``bulk``: A :symbol:`mongoc_bulk_operation_t <mongoc_bulk_operation_t>`.
* ``document``: A :symbol:`bson_t <bson:bson_t>`.

See Also
--------

:symbol:`Bulk Insert <bulk-insert>`

Errors
------

Errors are propagated via :symbol:`mongoc_bulk_operation_execute() <mongoc_bulk_operation_execute>`.

