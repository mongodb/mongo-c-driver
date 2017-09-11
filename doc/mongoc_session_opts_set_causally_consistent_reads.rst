:man_page: mongoc_session_opts_set_causally_consistent_reads

mongoc_session_opts_set_causally_consistent_reads()
===================================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_session_opts_set_causally_consistent_reads (mongoc_session_opt_t *opts,
                                                     bool causally_consistent_reads);

Configure causally consistent reads in a session. The default is false. If true, each read operations in the session will be causally ordered after the previous read or write operation. See the example code for :symbol:`mongoc_client_session_t`.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.
* ``causally_consistent_reads``: True or false.

.. only:: html

  .. taglist:: See Also:
    :tags: session
