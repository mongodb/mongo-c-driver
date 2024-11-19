:man_page: mongoc_structured_log_set_handler

mongoc_structured_log_set_handler
=================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_structured_log_set_handler (mongoc_structured_log_func_t log_func, void *user_data);

Sets the function to be called to handle structured log messages, as a :symbol:`mongoc_structured_log_func_t`.

The callback is given a :symbol:`mongoc_structured_log_entry_t` as a handle for obtaining additional information about the log message.
This entry pointer is only valid during a callback, because it's a low cost reference to temporary data.

This function is thread-safe.
It provides atomic updates to the handler function and data.
If handlers are ongoing, blocks until they complete before applying the change.

The callback handler itself must be thread-safe.
Logging can occur simultaneously on multiple application or background threads.

Handlers must not re-enter libmongoc except as allowed to access the log entry parameter.
See :symbol:`mongoc_structured_log_func_t`.

There is a single global handler per process.
Any call to this function will replace the default handler.
If the ``log_func`` is set to NULL, structured logging will be disabled.

Parameters
----------

* ``log_func``: The handler to install, a :symbol:`mongoc_structured_log_func_t`, or NULL to disable structured logging.
* ``user_data``: Optional user data, passed on to the handler.

.. seealso::

  | :doc:`structured_log`
