:man_page: mongoc_structured_log_opts_set_max_document_length_from_env

mongoc_structured_log_opts_set_max_document_length_from_env()
=============================================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_structured_log_opts_set_max_document_length_from_env (mongoc_structured_log_opts_t *opts);

Sets a maximum document length from the ``MONGODB_LOG_MAX_DOCUMENT_LENGTH`` environment variable, if a valid setting is found.
See :symbol:`mongoc_structured_log_opts_new` for a description of the supported environment variable formats.

Parse errors may cause a warning message, delivered via unstructured logging.

This happens automatically when :symbol:`mongoc_structured_log_opts_new` establishes defaults.
Any subsequent programmatic modifications to the :symbol:`mongoc_structured_log_opts_t` will override the environment variable settings.
For applications that desire the opposite behavior, where environment variables may override programmatic settings, they may call ``mongoc_structured_log_opts_set_max_document_length_from_env()`` after calling :symbol:`mongoc_structured_log_opts_set_max_document_length`.
This will process the environment a second time, allowing it to override customized defaults.

Returns
-------

Returns ``true`` on success: either a valid environment setting was found, or the value is unset and ``opts`` will not be modified.
If warnings are encountered in the environment, returns ``false`` and may log additional information to the unstructured logging facility.
Note that, by design, these errors are by default non-fatal.
When :symbol:`mongoc_structured_log_opts_new` internally calls this function, it ignores the return value.

.. seealso::

  | :doc:`structured_log`
