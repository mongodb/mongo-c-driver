:man_page: mongoc_structured_log

Structured Logging
==================

This document describes a newer "structured" logging facility which reports messages from the driver itself using a BSON format defined across driver implementations by the `MongoDB Logging Specification <https://specifications.readthedocs.io/en/latest/logging/logging/>`_.
See :doc:`unstructured_log` for the original freeform logging facility.

These two systems are configured and used independently. The structured logging system has independent settings for handler and log levels. 

Defaults
--------

By default, it follows the behavior outlined by the common `MongoDB Logging Specification <https://specifications.readthedocs.io/en/latest/logging/logging/>`_:

* Log levels are set from the environment variables ``MONGODB_LOG_ALL``, ``MONGODB_LOG_COMMAND``, ``MONGODB_LOG_TOPOLOGY``, ``MONGODB_LOG_SERVER_SELECTION``, expecting a value from the `severity level table <https://specifications.readthedocs.io/en/latest/logging/logging/#log-severity-levels>`_: ``off``, ``emergency``, ``alert``, ``critical``, ``error``, ``warning``, ``warn``, ``notice``, ``informational``, ``info``, ``debug``, ``trace``.
* A default handler is installed, which logs text representations of each message to a file given by ``MONGODB_LOG_PATH``, which may be a full path or one of the special values ``stdout`` or ``stderr``. By default, logs go to stderr.

Levels and Components
---------------------

Log levels and components are defined as :symbol:`mongoc_structured_log_level_t` and :symbol:`mongoc_structured_log_component_t` enumerations. Utilities are provided to convert between these values and their standard string representations. The string values are case-insensitive.

.. code-block:: c

  typedef enum {
    MONGOC_STRUCTURED_LOG_LEVEL_EMERGENCY = 0,
    MONGOC_STRUCTURED_LOG_LEVEL_ALERT = 1,
    MONGOC_STRUCTURED_LOG_LEVEL_CRITICAL = 2,
    MONGOC_STRUCTURED_LOG_LEVEL_ERROR = 3,
    MONGOC_STRUCTURED_LOG_LEVEL_WARNING = 4,
    MONGOC_STRUCTURED_LOG_LEVEL_NOTICE = 5,
    MONGOC_STRUCTURED_LOG_LEVEL_INFO = 6,
    MONGOC_STRUCTURED_LOG_LEVEL_DEBUG = 7,
    MONGOC_STRUCTURED_LOG_LEVEL_TRACE = 8,
  } mongoc_structured_log_level_t;

  typedef enum {
    MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND = 0,
    MONGOC_STRUCTURED_LOG_COMPONENT_TOPOLOGY = 1,
    MONGOC_STRUCTURED_LOG_COMPONENT_SERVER_SELECTION = 2,
    MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION = 3,
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


Log Filtering
-------------

Structured log messages may be filtered in two ways:

* A maximum log level can be set per-component.
* A log handler function can ignore messages based on any criteria.

The max level settings are configured by the environment variables above, and they may be altered or queried by applications.
To reduce overhead for unwanted logging, messages should be ignored as early as possible.
If it's possible to filter messages based on only their component and level, this should be done by handlers before requesting BSON serialization by calling :symbol:`mongoc_structured_log_entry_message_as_bson`.

.. toctree::
  :titlesonly:
  :maxdepth: 1

  mongoc_structured_log_set_max_level_for_all_components
  mongoc_structured_log_set_max_level_for_component
  mongoc_structured_log_get_max_level_for_component

Log Handlers
------------

There can be one global log handler set at a time.
This handler is called with a :symbol:`mongoc_structured_log_entry_t` that can be queried for further details.

.. note::

        Structured log handlers must be thread-safe.
        This differs from unstructured logging, which provides a global mutex.

.. toctree::
  :titlesonly:
  :maxdepth: 1

  mongoc_structured_log_func_t
  mongoc_structured_log_set_handler

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
