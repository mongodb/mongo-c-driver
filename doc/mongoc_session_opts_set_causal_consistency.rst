:man_page: mongoc_session_opts_set_causal_consistency

mongoc_session_opts_set_causal_consistency()
============================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_session_opts_set_causal_consistency (mongoc_session_opt_t *opts,
                                              bool causal_consistency);

Configure causal consistency in a session. The default is false. If true, each read operations in the session will be causally ordered after the previous read or write operation. See the example code for :symbol:`mongoc_client_session_t`.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.
* ``causal_consistency``: True or false.

.. only:: html

  .. taglist:: See Also:
    :tags: session
