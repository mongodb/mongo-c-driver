:man_page: mongoc_unstructured_log

Unstructured Logging
====================

This is the original logging facility that supports freeform string messages originating from the driver itself or from application code. This has been retroactively termed "unstructured logging".
See :doc:`structured_log` for the newer standardized logging facility.

.. code-block:: c

  typedef enum {
     MONGOC_LOG_LEVEL_ERROR,
     MONGOC_LOG_LEVEL_CRITICAL,
     MONGOC_LOG_LEVEL_WARNING,
     MONGOC_LOG_LEVEL_MESSAGE,
     MONGOC_LOG_LEVEL_INFO,
     MONGOC_LOG_LEVEL_DEBUG,
     MONGOC_LOG_LEVEL_TRACE,
  } mongoc_log_level_t;

  #define MONGOC_ERROR(...)
  #define MONGOC_CRITICAL(...)
  #define MONGOC_WARNING(...)
  #define MONGOC_MESSAGE(...)
  #define MONGOC_INFO(...)
  #define MONGOC_DEBUG(...)

  typedef void (*mongoc_log_func_t) (mongoc_log_level_t log_level,
                                     const char *log_domain,
                                     const char *message,
                                     void *user_data);

  void
  mongoc_log_set_handler (mongoc_log_func_t log_func, void *user_data);
  void
  mongoc_log (mongoc_log_level_t log_level,
              const char *log_domain,
              const char *format,
              ...);
  const char *
  mongoc_log_level_str (mongoc_log_level_t log_level);
  void
  mongoc_log_default_handler (mongoc_log_level_t log_level,
                              const char *log_domain,
                              const char *message,
                              void *user_data);
  void
  mongoc_log_trace_enable (void);
  void
  mongoc_log_trace_disable (void);

This abstraction can be used for logging in your application, or you can integrate the driver with an existing logging system.

Macros
------

To make logging a little less painful, various helper macros are provided. See the following example.

.. code-block:: c

  #undef MONGOC_LOG_DOMAIN
  #define MONGOC_LOG_DOMAIN "my-custom-domain"

  MONGOC_WARNING ("An error occurred: %s", strerror (errno));

.. _custom_log_handlers:

Custom Log Handlers
-------------------

The default log handler prints a timestamp and the log message to ``stdout``, or to ``stderr`` for warnings, critical messages, and errors.
You can override the handler with ``mongoc_log_set_handler()``.
Your handler function is called in a mutex for thread safety.

For example, you could register a custom handler to suppress messages at INFO level and below:

.. code-block:: c

  void
  my_logger (mongoc_log_level_t log_level,
             const char *log_domain,
             const char *message,
             void *user_data)
  {
     /* smaller values are more important */
     if (log_level < MONGOC_LOG_LEVEL_INFO) {
        mongoc_log_default_handler (log_level, log_domain, message, user_data);
     }
  }

  int
  main (int argc, char *argv[])
  {
     mongoc_log_set_handler (my_logger, NULL);
     mongoc_init ();

     /* ... your code ...  */

     mongoc_cleanup ();
     return 0;
  }

Note that in the example above ``mongoc_log_set_handler()`` is called before ``mongoc_init()``.
Otherwise, some log traces could not be processed by the log handler.

To restore the default handler:

.. code-block:: c

  mongoc_log_set_handler (mongoc_log_default_handler, NULL);

Disable logging
---------------

To disable all logging, including warnings, critical messages and errors, provide an empty log handler:

.. code-block:: c

  mongoc_log_set_handler (NULL, NULL);

Tracing
-------

If compiling your own copy of the MongoDB C driver, consider configuring with ``-DENABLE_TRACING=ON`` to enable function tracing and hex dumps of network packets to ``STDERR`` and ``STDOUT`` during development and debugging.

This is especially useful when debugging what may be going on internally in the driver.

Trace messages can be enabled and disabled by calling ``mongoc_log_trace_enable()`` and ``mongoc_log_trace_disable()``

.. note::

        Compiling the driver with ``-DENABLE_TRACING=ON`` will affect its performance. Disabling tracing with ``mongoc_log_trace_disable()`` significantly reduces the overhead, but cannot remove it completely.
