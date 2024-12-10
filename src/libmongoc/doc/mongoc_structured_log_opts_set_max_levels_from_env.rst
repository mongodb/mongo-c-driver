:man_page: mongoc_structured_log_opts_set_max_levels_from_env

mongoc_structured_log_opts_set_max_levels_from_env()
====================================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_structured_log_opts_set_max_levels_from_env (mongoc_structured_log_opts_t *opts);

Sets any maximum log levels requested by environment variables: ``MONGODB_LOG_ALL`` for all components, followed by per-component log levels ``MONGODB_LOG_COMMAND``, ``MONGODB_LOG_CONNECTION``, ``MONGODB_LOG_TOPOLOGY``, and ``MONGODB_LOG_SERVER_SELECTION``.

Expects the value to be recognizable by :symbol:`mongoc_structured_log_get_named_level`.
Parse errors may cause a warning message, delivered via unstructured logging.

Component levels with no valid environment variable setting will be left unmodified.

This happens automatically when :symbol:`mongoc_structured_log_opts_new` establishes defaults.
Any subsequent programmatic modifications to the :symbol:`mongoc_structured_log_opts_t` will override the environment variable settings.
For applications that desire the opposite behavior, where environment variables may override programmatic settings, they may call ``mongoc_structured_log_opts_set_max_levels_from_env()`` after calling :symbol:`mongoc_structured_log_opts_set_max_level_for_component` and :symbol:`mongoc_structured_log_opts_set_max_level_for_all_components`.
This will process the environment a second time, allowing it to override customized defaults.

Returns
-------

Returns ``true`` on success.
If warnings are encountered in the environment, returns ``false`` and may log additional information to the unstructured logging facility.
Note that, by design, these errors are by default non-fatal.
When :symbol:`mongoc_structured_log_opts_new` internally calls this function, it ignores the return value.

.. seealso::

  | :doc:`structured_log`
