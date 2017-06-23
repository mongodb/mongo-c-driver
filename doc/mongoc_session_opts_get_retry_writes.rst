:man_page: mongoc_session_opts_get_retry_writes

:tags: session

mongoc_session_opts_get_retry_writes()
======================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_session_opts_get_retry_writes (mongoc_session_opt_t *opts);

Return true if this session is configured for retryable writes, else false (the default). See :symbol:`mongoc_session_opts_set_retry_writes()`.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.

.. only:: html

  .. taglist:: See Also:
    :tags: session
