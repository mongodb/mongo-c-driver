:man_page: mongoc_bulk_operation_set_client_session

mongoc_bulk_operation_set_client_session()
==========================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_bulk_operation_set_client_session (
     mongoc_bulk_operation_t *bulk, mongoc_client_session_t *client_session);

Parameters
----------

* ``bulk``: A :symbol:`mongoc_bulk_operation_t`.
* ``client_session``: A :symbol:`mongoc_client_session_t`.

Description
-----------

Specifies a client session to use for the operation. This function has an effect only if called before :symbol:`mongoc_bulk_operation_execute`.

If a :symbol:`mongoc_client_t` has already been assigned with :symbol:`mongoc_bulk_operation_set_client`, the ``client_session`` parameter must be associated with the same :symbol:`mongoc_client_t`.

Use ``mongoc_bulk_operation_set_client_session`` only for building a language driver that wraps the C Driver. When writing applications in C, a client session should be specified via :symbol:`mongoc_collection_create_bulk_operation_with_opts`.
