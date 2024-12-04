:man_page: mongoc_structured_log_opts_set_handler

mongoc_structured_log_opts_set_handler()
========================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_structured_log_opts_set_handler (mongoc_structured_log_opts_t *opts,
                                          mongoc_structured_log_func_t log_func,
                                          void *user_data);

Sets the function to be called to handle structured log messages, as a :symbol:`mongoc_structured_log_func_t`.

The callback is given a :symbol:`mongoc_structured_log_entry_t` as a handle for obtaining additional information about the log message.
This entry pointer is only valid during a callback, because it's a low cost reference to temporary data.

Structured log handlers must be thread-safe if they will be used with :symbol:`mongoc_client_pool_t`.
Handlers must avoid unbounded recursion, preferably by avoiding the use of any ``libmongoc`` client or pool which uses the same handler.

This function always replaces the default log handler from :symbol:`mongoc_structured_log_opts_new`, if it was still set.
If the ``log_func`` is set to NULL, structured logging will be disabled.

Parameters
----------

* ``opts``: Structured log options, allocated with :symbol:`mongoc_structured_log_opts_new`.
* ``log_func``: The handler to install, a :symbol:`mongoc_structured_log_func_t`, or NULL to disable structured logging.
* ``user_data``: Optional user data, passed on to the handler.

.. seealso::

  | :doc:`structured_log`
