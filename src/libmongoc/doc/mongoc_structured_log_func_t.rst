:man_page: mongoc_structured_log_func_t

mongoc_structured_log_func_t
============================

Synopsis
--------

.. code-block:: c

  typedef void (*mongoc_structured_log_func_t)
  (const mongoc_structured_log_entry_t *entry, void *user_data);

Callback function for :symbol:`mongoc_structured_log_opts_set_handler`.
Structured log handlers must be thread-safe if they will be used with :symbol:`mongoc_client_pool_t`.
Handlers must avoid unbounded recursion, preferably by avoiding the use of any ``libmongoc`` client or pool which uses the same handler.

Parameters
----------

* ``entry``: A :symbol:`mongoc_structured_log_entry_t` pointer, only valid during the handler invocation.
* ``user_data``: Optional user data from :symbol:`mongoc_structured_log_opts_set_handler`.

.. seealso::

  | :doc:`structured_log`
