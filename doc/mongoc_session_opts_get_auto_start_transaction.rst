:man_page: mongoc_session_opts_get_auto_start_transaction

mongoc_session_opts_get_auto_start_transaction()
================================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_session_opts_get_auto_start_transaction (const mongoc_session_opt_t *opts);

Return true if this session is configured to automatically start multi-document transactions. See :symbol:`mongoc_session_opts_set_auto_start_transaction()`.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.

.. only:: html

  .. taglist:: See Also:
    :tags: session
