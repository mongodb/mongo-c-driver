:man_page: mongoc_client_session_start_transaction

mongoc_client_session_start_transaction()
=========================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_client_session_start_transaction (mongoc_client_session_t *session,
                                           mongoc_transaction_opt_t *opts,
                                           bson_error_t *error);


Start a multi-document transaction for all following operations in this session. Any options provided in ``opts`` override options passed to :symbol:`mongoc_session_opts_set_default_transaction_opts`.

The transaction must be completed with :symbol:`mongoc_client_session_commit_transaction` or :symbol:`mongoc_client_session_abort_transaction`. An in-progress transaction is automatically aborted by :symbol:`mongoc_client_session_destroy`.

Parameters
----------

* ``session``: A :symbol:`mongoc_client_session_t`.
* ``opts``: A :symbol:`mongoc_transaction_opt_t` or ``NULL``.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Return
------

Returns true if the transaction was started. Returns ``false`` and sets ``error`` if there are invalid arguments, such as a session with a transaction already in progress.

.. only:: html

  .. taglist:: See Also:
    :tags: session
