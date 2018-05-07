:man_page: mongoc_session_opts_set_default_transaction_opts

mongoc_session_opts_set_default_transaction_opts()
==================================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_session_opts_set_default_transaction_opts (
     mongoc_session_opt_t *opts, const mongoc_transaction_opt_t *txn_opts);

Set the default options for transactions started with this session. Useful with :symbol:`mongoc_session_opts_set_auto_start_transaction`.

The ``txn_opts`` argument is copied and can be freed after calling this function.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.
* ``txn_opts``: A :symbol:`mongoc_transaction_opt_t`.

.. only:: html

  .. taglist:: See Also:
    :tags: session
