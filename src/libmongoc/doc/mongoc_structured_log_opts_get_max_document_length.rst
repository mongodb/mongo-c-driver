:man_page: mongoc_structured_log_opts_get_max_document_length

mongoc_structured_log_opts_get_max_document_length()
====================================================

Synopsis
--------

.. code-block:: c

  size_t
  mongoc_structured_log_opts_get_max_document_length (const mongoc_structured_log_opts_t *opts);

Parameters
----------

* ``opts``: Structured log options, allocated with :symbol:`mongoc_structured_log_opts_new`.

Returns
-------

Returns the current maximum document length set in ``opts``, as a ``size_t``. Always succeeds.
This may be the last value set with :symbol:`mongoc_structured_log_opts_set_max_document_length` or it may be an environment variable captured by :symbol:`mongoc_structured_log_opts_set_max_document_length_from_env` or :symbol:`mongoc_structured_log_opts_new`.

.. seealso::

  | :doc:`structured_log`
