:man_page: mongoc_bulk_operation_delete_one

mongoc_bulk_operation_delete_one()
==================================

.. warning::
   .. deprecated:: 0.96.0

      Use :symbol:`mongoc_bulk_operation_remove_one()` instead.

Synopsis
--------

.. code-block:: c

  void
  mongoc_bulk_operation_delete_one (mongoc_bulk_operation_t *bulk,
                                    const bson_t *selector);

Delete a single document as part of a bulk operation. This only queues the operation. To execute it, call :symbol:`mongoc_bulk_operation_execute()`.

Parameters
----------

* ``bulk``: A :symbol:`mongoc_bulk_operation_t`.
* ``selector``: A :symbol:`bson:bson_t`.

Errors
------

Errors are propagated via :symbol:`mongoc_bulk_operation_execute()`.

.. seealso::

  | :symbol:`mongoc_bulk_operation_remove_one_with_opts()`

  | :symbol:`mongoc_bulk_operation_remove_many_with_opts()`

