:man_page: mongoc_bulk_operation_set_client

mongoc_bulk_operation_set_client()
==================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_bulk_operation_set_client (mongoc_bulk_operation_t *bulk, void *client);

Parameters
----------

* ``bulk``: A :symbol:`mongoc_bulk_operation_t`.
* ``client``: A :symbol:`mongoc_client_t`.

Description
-----------

Specifies a client to use for the operation. This function has an effect only if called before :symbol:`mongoc_bulk_operation_execute`.

If a :symbol:`mongoc_client_session_t` has already been assigned with :symbol:`mongoc_bulk_operation_set_client_session`, the ``client`` parameter must be the same as the :symbol:`mongoc_client_t` associated with that :symbol:`mongoc_client_session_t`.

Use ``mongoc_bulk_operation_set_client`` only for building a language driver that wraps the C Driver. When writing applications in C, a client is implicitly set by :symbol:`mongoc_collection_create_bulk_operation_with_opts`.

The ``client`` parameter's type is a void pointer by mistake, but it must be preserved for ABI. You must only pass a :symbol:`mongoc_client_t` pointer for ``client``.
