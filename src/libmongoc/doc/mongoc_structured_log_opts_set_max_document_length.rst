:man_page: mongoc_structured_log_opts_set_max_document_length

mongoc_structured_log_opts_set_max_document_length()
====================================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_structured_log_opts_set_max_document_length (mongoc_structured_log_opts_t *opts,
                                                       size_t max_document_length);

Sets a maximum length for BSON documents that appear serialized in JSON form as part of a structured log message.

This setting is captured from the environment during :symbol:`mongoc_structured_log_opts_new` if a valid setting is found for the ``MONGODB_LOG_MAX_DOCUMENT_LENGTH`` environment variable.

Serialized JSON will be truncated at this limit, interpreted as a count of UTF-8 encoded bytes. Truncation will be indicated with a ``...`` suffix, the length of which is not included in the max document length. If truncation at the exact indicated length would split a valid UTF-8 sequence, we instead truncate the document earlier at the nearest boundary between code points.

Parameters
----------

* ``opts``: Structured log options, allocated with :symbol:`mongoc_structured_log_opts_new`.
* ``max_document_length``: Maximum length for each embedded JSON document, in bytes, not including an ellipsis (``...``) added to indicate truncation. Values near or above ``INT_MAX`` will be rejected.

Returns
-------

Returns ``true`` on success, or ``false`` if the supplied maximum length is too large.

.. seealso::

  | :doc:`structured_log`
