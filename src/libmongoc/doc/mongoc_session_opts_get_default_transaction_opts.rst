:man_page: mongoc_session_opts_get_default_transaction_opts

mongoc_session_opts_get_default_transaction_opts()
==================================================

Synopsis
--------

.. code-block:: c

  const mongoc_transaction_opt_t *
  mongoc_session_opts_get_default_transaction_opts (const mongoc_session_opt_t *opts);

The default options for transactions started with this session.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.

Returns
-------

A :symbol:`mongoc_transaction_opt_t` which should not be modified or freed.

See :symbol:`mongoc_session_opts_set_default_transaction_opts()`.

.. only:: html

  .. include:: includes/seealso/session.txt
