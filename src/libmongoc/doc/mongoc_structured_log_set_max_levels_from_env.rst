:man_page: mongoc_structured_log_set_max_levels_from_env

mongoc_structured_log_set_max_levels_from_env
=============================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_structured_log_set_max_levels_from_env (void)

Sets any maximum log levels requested by environment variables: ``MONGODB_LOG_ALL`` for all components, followed by per-component log levels ``MONGODB_LOG_COMMAND``, ``MONGODB_LOG_CONNECTION``, ``MONGODB_LOG_TOPOLOGY``, and ``MONGODB_LOG_SERVER_SELECTION``.

Expects the value to be recognizable by :symbol:`mongoc_structured_log_get_named_level`.
Parse errors may cause a warning message.

Component levels with no valid environment variable setting will be left unmodified.

Normally this happens automatically during :symbol:`mongoc_init`, and it provides defaults that can be overridden programmatically by calls to :symbol:`mongoc_structured_log_set_max_level_for_component` and :symbol:`mongoc_structured_log_set_max_level_for_all_components`.

For applications that desire the opposite behavior, where environment variables may override programmatic settings, they may call ``mongoc_structured_log_set_max_levels_from_env()`` after calling :symbol:`mongoc_structured_log_set_max_level_for_component` and :symbol:`mongoc_structured_log_set_max_level_for_all_components`.
This will process the environment a second time, allowing it to override customized defaults.

.. seealso::

  | :doc:`structured_log`
