:man_page: mongoc_session_opts_set_retry_writes

mongoc_session_opts_set_retry_writes()
======================================

Synopsis
--------


.. code-block:: c

  void
  mongoc_session_opts_set_retry_writes (mongoc_session_opt_t *opts,
                                        bool retry_writes);

Configure retryable writes in a session. The default is false. If true, all write operations which can be safely retried will be retried once after a network error. It is an error to attempt any write operation which is not safely retryable within this session. See the example code for :symbol:`mongoc_session_t`.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.
* ``retry_writes``: True or false.

.. only:: html

  .. taglist:: See Also:
    :tags: session
