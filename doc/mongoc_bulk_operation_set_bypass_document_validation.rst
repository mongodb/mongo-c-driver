:man_page: mongoc_bulk_operation_set_bypass_document_validation

mongoc_bulk_operation_set_bypass_document_validation()
======================================================

Synopsis
--------

.. code-block:: none

  void
  mongoc_bulk_operation_set_bypass_document_validation (mongoc_bulk_operation_t   *bulk,
                                                        bool                       bypass);

Parameters
----------

* ``bulk``: A :symbol:`mongoc_bulk_operation_t <mongoc_bulk_operation_t>`.
* ``bypass``: A boolean.

Description
-----------

Will bypass document validation for all operations part of this :doc:`bulk <mongoc_bulk_operation_t>`.

See Also
--------

:symbol:`Bulk Operation Bypassing Document Validation <bulk-write-bypass-document-validation>`

