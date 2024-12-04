:man_page: mongoc_structured_log

Structured Logging
==================

This document describes a newer "structured" logging facility which reports messages from the driver itself using a BSON format defined across driver implementations by the `MongoDB Logging Specification <https://specifications.readthedocs.io/en/latest/logging/logging/>`_.
See :doc:`unstructured_log` for the original freeform logging facility.

These two systems are configured and used independently.

Unstructured logging is global to the entire process, but structured logging is configured separately for each :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t`.
See :symbol:`mongoc_client_set_structured_log_opts` and :symbol:`mongoc_client_pool_set_structured_log_opts`.

Options
-------

Structured log settings are tracked explicitly by a :symbol:`mongoc_structured_log_opts_t` instance.

Like other drivers supporting structured logging, we take default settings from environment variables and offer additional optional programmatic configuration.
Environment variables are captured during :symbol:`mongoc_structured_log_opts_new`, refer there for a full list of the supported variables.

Normally environment variables provide defaults that can be overridden programmatically.
To request the opposite behavior, where your programmatic defaults can be overridden by the environment, see :symbol:`mongoc_structured_log_opts_set_max_levels_from_env`.

Structured log messages may be filtered in arbitrary ways by the handler, but as both a performance optimization and a convenience, a built-in filter limits the maximum log level of reported messages with a per-component setting.

.. toctree::
  :titlesonly:
  :maxdepth: 1

  mongoc_structured_log_opts_t
  mongoc_structured_log_opts_new
  mongoc_structured_log_opts_destroy
  mongoc_structured_log_opts_set_handler
  mongoc_structured_log_opts_get_max_level_for_component
  mongoc_structured_log_opts_set_max_level_for_component
  mongoc_structured_log_opts_set_max_level_for_all_components
  mongoc_structured_log_opts_set_max_levels_from_env


Levels and Components
---------------------

Log levels and components are defined as :symbol:`mongoc_structured_log_level_t` and :symbol:`mongoc_structured_log_component_t` enumerations. Utilities are provided to convert between these values and their standard string representations. The string values are case-insensitive.

.. code-block:: c

  typedef enum {
    MONGOC_STRUCTURED_LOG_LEVEL_EMERGENCY = 0,  // "Emergency" ("off" also accepted)
    MONGOC_STRUCTURED_LOG_LEVEL_ALERT = 1,      // "Alert"
    MONGOC_STRUCTURED_LOG_LEVEL_CRITICAL = 2,   // "Critical"
    MONGOC_STRUCTURED_LOG_LEVEL_ERROR = 3,      // "Error"
    MONGOC_STRUCTURED_LOG_LEVEL_WARNING = 4,    // "Warning" ("warn" also accepted)
    MONGOC_STRUCTURED_LOG_LEVEL_NOTICE = 5,     // "Notice"
    MONGOC_STRUCTURED_LOG_LEVEL_INFO = 6,       // "Informational" ("info" also accepted)
    MONGOC_STRUCTURED_LOG_LEVEL_DEBUG = 7,      // "Debug"
    MONGOC_STRUCTURED_LOG_LEVEL_TRACE = 8,      // "Trace"
  } mongoc_structured_log_level_t;

  typedef enum {
    MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND = 0,           // "command"
    MONGOC_STRUCTURED_LOG_COMPONENT_TOPOLOGY = 1,          // "topology"
    MONGOC_STRUCTURED_LOG_COMPONENT_SERVER_SELECTION = 2,  // "serverSelection"
    MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION = 3,        // "connection"
  } mongoc_structured_log_component_t;

.. toctree::
  :titlesonly:
  :maxdepth: 1

  mongoc_structured_log_level_t
  mongoc_structured_log_component_t

.. seealso::

  mongoc_structured_log_get_level_name
  mongoc_structured_log_get_named_level
  mongoc_structured_log_get_component_name
  mongoc_structured_log_get_named_component


Log Handlers
------------

Each :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` has its own instance of the structured logging subsystem, with its own settings and handler.

When using :symbol:`mongoc_client_pool_t`, the pooled clients all share a common logging instance. Handlers must be thread-safe.

The handler is called for each log entry with a level no greater than its component's maximum.
A :symbol:`mongoc_structured_log_entry_t` pointer provides access to further details, during the handler only.

Handlers must take care not to re-enter ``libmongoc`` with the same :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` that the handler has been called by.

.. toctree::
  :titlesonly:
  :maxdepth: 1

  mongoc_structured_log_func_t

Log Entries
-----------

Each log entry is represented within the handler by a short-lived :symbol:`mongoc_structured_log_entry_t` pointer.
During the handler, this pointer can be used to access the individual properties of an entry: its level, component, and message.

The message will be assembled as a :symbol:`bson_t` only when explicitly requested by a call to :symbol:`mongoc_structured_log_entry_message_as_bson`.
This results in a standalone document that may be retained for any amount of time and must be explicitly destroyed.

.. toctree::
  :titlesonly:
  :maxdepth: 1

  mongoc_structured_log_entry_t

Example
-------
.. literalinclude:: ../examples/example-structured-log.c
   :language: c
   :caption: example-structured-log.c

.. seealso::

  mongoc_structured_log_entry_get_component
  mongoc_structured_log_entry_get_level
  mongoc_structured_log_entry_message_as_bson
