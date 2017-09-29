:man_page: mongoc_session_opts_get_causally_consistent_reads

mongoc_session_opts_get_causally_consistent_reads()
===================================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_session_opts_get_causally_consistent_reads (mongoc_session_opt_t *opts);

Return true if this session is configured for causally consistent reads, else false (the default). See :symbol:`mongoc_session_opts_set_causally_consistent_reads()`.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.

.. only:: html

  .. taglist:: See Also:
    :tags: session
