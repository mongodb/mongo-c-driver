:man_page: mongoc_bulk_operation_get_hint

mongoc_bulk_operation_get_hint()
================================

.. warning::
   .. deprecated:: 1.28.0

      This function is deprecated and should not be used in new code.

      Please use :symbol:`mongoc_bulk_operation_get_server_id()` in new code.

Synopsis
--------

.. code-block:: c

  uint32_t
  mongoc_bulk_operation_get_hint (const mongoc_bulk_operation_t *bulk)
    BSON_GNUC_DEPRECATED_FOR (mongoc_bulk_operation_get_server_id);

Parameters
----------

* ``bulk``: A :symbol:`mongoc_bulk_operation_t`.

Description
-----------

Retrieves the opaque id of the server used for the operation.

(The function name includes the old term "hint" for the sake of backward compatibility, but we now call this number a "server id".)

This number is zero until the driver actually uses a server in :symbol:`mongoc_bulk_operation_execute`. The server id is the same number as the return value of a successful :symbol:`mongoc_bulk_operation_execute`, so ``mongoc_bulk_operation_get_hint`` is useful mainly in case :symbol:`mongoc_bulk_operation_execute` fails and returns zero.

