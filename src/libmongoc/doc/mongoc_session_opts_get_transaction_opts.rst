:man_page: mongoc_session_opts_get_transaction_opts

mongoc_session_opts_get_transaction_opts()
==========================================

Synopsis
--------

.. code-block:: c

   mongoc_transaction_opt_t *
   mongoc_session_opts_get_transaction_opts (const mongoc_client_session_t *session);

The options for the current transaction started with this session. The resulting :symbol:`mongoc_transaction_opt_t` should be freed with :symbol:`mongoc_transaction_opts_destroy`. If this ``session`` is not in a transaction, then the returned value is ``NULL``. See :symbol:`mongoc_client_session_in_transaction()`. 

Parameters
----------

* ``session``: A :symbol:`mongoc_client_session_t`.

Returns
-------

A newly allocated :symbol:`mongoc_transaction_opt_t` that should be freed with :symbol:`mongoc_transaction_opts_destroy` or ``NULL`` if the session is not in a transaction.

.. only:: html

  .. include:: includes/seealso/session.txt

