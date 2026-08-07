:man_page: mongoc_bulk_operation_get_bypass_document_validation

mongoc_bulk_operation_get_bypass_document_validation()
======================================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_bulk_operation_get_bypass_document_validation (
     mongoc_bulk_operation_t *bulk);

Parameters
----------

* ``bulk``: A :symbol:`mongoc_bulk_operation_t`.

Description
-----------

Whether to bypass document validation for all operations part of this :doc:`bulk <mongoc_bulk_operation_t>`.

.. seealso::

  | `Bulk Operation Bypassing Document Validation <bulk_operation_bypassing_document_validation_>`_

