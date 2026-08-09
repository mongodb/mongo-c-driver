:man_page: mongoc_bulk_operation_get_comment

mongoc_bulk_operation_get_comment()
===================================

Synopsis
--------

.. code-block:: c

  const bson_value_t*
  mongoc_bulk_operation_get_comment (mongoc_bulk_operation_t *bulk);

Parameters
----------

* ``bulk``: A :symbol:`mongoc_bulk_operation_t`.

Description
-----------

Retrieves the comment to attach to all commands executed as part of this :doc:`bulk <mongoc_bulk_operation_t>`. The comment will appear in log messages, profiler output, and currentOp output.

Returns
-------

A bson_value_t that should not be modified or freed.
