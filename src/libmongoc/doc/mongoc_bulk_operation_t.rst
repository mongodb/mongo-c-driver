:man_page: mongoc_bulk_operation_t

mongoc_bulk_operation_t
=======================

Bulk Write Operations

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_bulk_operation_t mongoc_bulk_operation_t;

Description
-----------

:symbol:`mongoc_bulk_operation_t` provides an abstraction for submitting multiple write operations as a single batch.

After adding all of the write operations to the :symbol:`mongoc_bulk_operation_t`, call
:symbol:`mongoc_bulk_operation_execute()` to execute the operation.

.. warning::

  It is only valid to call :symbol:`mongoc_bulk_operation_execute()` once. The ``mongoc_bulk_operation_t`` must be destroyed afterwards.

.. seealso::

  | `Bulk Write Operations <bulk_>`_
  | :symbol:`mongoc_bulkwrite_t`

.. note::

  .. include:: includes/bulkwrite-vs-bulk_operation.txt

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_bulk_operation_delete
    mongoc_bulk_operation_delete_one
    mongoc_bulk_operation_destroy
    mongoc_bulk_operation_execute
    mongoc_bulk_operation_get_hint
    mongoc_bulk_operation_get_server_id
    mongoc_bulk_operation_get_write_concern
    mongoc_bulk_operation_insert
    mongoc_bulk_operation_insert_with_opts
    mongoc_bulk_operation_remove
    mongoc_bulk_operation_remove_many_with_opts
    mongoc_bulk_operation_remove_one
    mongoc_bulk_operation_remove_one_with_opts
    mongoc_bulk_operation_replace_one
    mongoc_bulk_operation_replace_one_with_opts
    mongoc_bulk_operation_set_bypass_document_validation
    mongoc_bulk_operation_set_client_session
    mongoc_bulk_operation_set_comment
    mongoc_bulk_operation_set_hint
    mongoc_bulk_operation_set_server_id
    mongoc_bulk_operation_set_let
    mongoc_bulk_operation_update
    mongoc_bulk_operation_update_many_with_opts
    mongoc_bulk_operation_update_one
    mongoc_bulk_operation_update_one_with_opts

