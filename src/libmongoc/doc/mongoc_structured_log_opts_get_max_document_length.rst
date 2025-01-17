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

Returns the current maximum document length set in ``opts``, as a ``size_t``.

.. seealso::

  | :doc:`structured_log`
