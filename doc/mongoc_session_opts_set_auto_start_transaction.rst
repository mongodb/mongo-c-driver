:man_page: mongoc_session_opts_set_auto_start_transaction

mongoc_session_opts_set_auto_start_transaction()
================================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_session_opts_set_auto_start_transaction (mongoc_session_opt_t *opts,
                                                  bool auto_start_transaction);

Configure a session to automatically start a multi-document transaction. If true, any operation in the session will begin a transaction if one is not in progress. The default is false.

Provide options for automatically-started transactions with :symbol:`mongoc_session_opts_set_default_transaction_opts`.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.
* ``auto_start_transaction``: True or false.

.. only:: html

  .. taglist:: See Also:
    :tags: session
