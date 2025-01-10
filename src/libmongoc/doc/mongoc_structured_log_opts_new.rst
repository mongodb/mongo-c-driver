:man_page: mongoc_structured_log_opts_new

mongoc_structured_log_opts_new()
================================

Synopsis
--------

.. code-block:: c

  mongoc_structured_log_opts_t *
  mongoc_structured_log_opts_new (void);

Creates a new :symbol:`mongoc_structured_log_opts_t`, filled with defaults captured from the current environment.

Sets a default log handler which would write a text representation of each log message to ``stderr``, ``stdout``, or another file configurable using ``MONGODB_LOG_PATH``.
This setting has no effect if the default handler is replaced using :symbol:`mongoc_structured_log_opts_set_handler`.

Environment variable errors are non-fatal, and result in one-time warnings delivered as an unstructured log.

Per-component maximum levels are initialized equivalently to:

.. code-block:: c

  mongoc_structured_log_opts_set_max_level_for_all_components(opts, MONGOC_STRUCTURED_LOG_LEVEL_WARNING);
  mongoc_structured_log_opts_set_max_levels_from_env(opts);

Environment Variables
---------------------

This is a full list of the captured environment variables.

* ``MONGODB_LOG_MAX_DOCUMENT_LENGTH``: Maximum length for JSON-serialized documents that appear within a log message.
  It may be a number, in bytes, or ``unlimited`` (case insensitive).
  By default, the limit is 1000 bytes.
  This limit affects interior documents like commands and replies, not the total length of a structured log message.

* ``MONGODB_LOG_PATH``: A file path or one of the special strings ``stderr`` or ``stdout`` (case insensitive) specifying the destination for structured logs seen by the default handler.
  By default, it writes to ``stderr``.
  This path will be captured during ``mongoc_structured_log_opts_new()``, but it will not immediately be opened.
  If the file can't be opened, a warning is then written to the unstructured log and the handler writes structured logs to ``stderr`` instead.

  .. warning:: When a file path is given for ``MONGODB_LOG_PATH``, each log instance (one stand-alone client or pool) will separately open this file for append.
    The results are operating system specific. On UNIX-like platforms each instance's output will be interleaved, in most cases without splitting individual log messages. Notably on Windows the file will be opened in exclusive mode by the first instance and subsequent instances will fail, falling back on the default of ``stderr``.
    Applications that use multiple processes or multiple client pools will likely want to supply a log handler that annotates each message with information about its originating log instance.

* ``MONGODB_LOG_COMMAND``: A log level name to set as the maximum for ``MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND``.
* ``MONGODB_LOG_TOPOLOGY``: A log level name to set as the maximum for ``MONGOC_STRUCTURED_LOG_COMPONENT_TOPOLOGY``.
* ``MONGODB_LOG_SERVER_SELECTION``: A log level name to set as the maximum for ``MONGOC_STRUCTURED_LOG_COMPONENT_SERVER_SELECTION``.
* ``MONGODB_LOG_CONNECTION``: A log level name to set as the maximum for ``MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION``.
* ``MONGODB_LOG_ALL``: A log level name applied to all components not otherwise specified.

Note that log level names are always case insensitive.
This is a full list of recognized names, including allowed aliases:

* ``emergency``, ``off``
* ``alert``
* ``critical``
* ``error``
* ``warning``, ``warn``
* ``notice``
* ``informational``, ``info``
* ``debug``
* ``trace``

Returns
-------

A newly allocated :symbol:`mongoc_structured_log_opts_t`.
