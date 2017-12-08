:man_page: mongoc_collection_create_bulk_operation_with_opts

mongoc_collection_create_bulk_operation_with_opts()
===================================================

Synopsis
--------

.. code-block:: c

  mongoc_bulk_operation_t *
  mongoc_collection_create_bulk_operation_with_opts (
     mongoc_collection_t *collection,
     const bson_t *opts) BSON_GNUC_WARN_UNUSED_RESULT;

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``opts``: A :symbol:`bson:bson_t` containing additional options.

Description
-----------

This function shall begin a new bulk operation. After creating this you may call various functions such as :symbol:`mongoc_bulk_operation_update()`, :symbol:`mongoc_bulk_operation_insert()` and others.

After calling :symbol:`mongoc_bulk_operation_execute()` the commands will be executed in as large as batches as reasonable by the client.

If ``opts`` contains a field "ordered" with a true value, or no "ordered" value at all, then the bulk operation is ordered and processing will stop at the first error.

If ``opts`` contains a field "ordered" with a false value, then the bulk operation will attempt to continue processing even after the first failure.

All operations in the bulk operation will use the "writeConcern" field specified in ``opts``. If there is none then the collection's write concern is used. The global default is acknowledged writes: MONGOC_WRITE_CONCERN_W_DEFAULT.

If ``opts`` contains a "sessionId" field, which may be added with :symbol:`mongoc_client_session_append`, all operations in the bulk operation will use the corresponding :symbol:`mongoc_client_session_t`. See the example code for :symbol:`mongoc_client_session_t`.

See Also
--------

:symbol:`Bulk Write Operations <bulk>`

:symbol:`mongoc_bulk_operation_t`

Errors
------

Errors are propagated when executing the bulk operation.

Returns
-------

A newly allocated :symbol:`mongoc_bulk_operation_t` that should be freed with :symbol:`mongoc_bulk_operation_destroy()` when no longer in use.

.. warning::

  Failure to handle the result of this function is a programming error.

