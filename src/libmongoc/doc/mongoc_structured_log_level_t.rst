:man_page: mongoc_structured_log_level_t

mongoc_structured_log_level_t
=============================

Synopsis
--------

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

``mongoc_structured_log_level_t`` enumerates the available log levels for use with structured logging.

Functions
---------

.. toctree::
  :titlesonly:
  :maxdepth: 1

  mongoc_structured_log_get_level_name
  mongoc_structured_log_get_named_level

.. seealso::

  | :doc:`structured_log`
