:man_page: mongoc_structured_log_set_max_level_for_all_components

mongoc_structured_log_set_max_level_for_all_components
======================================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_structured_log_set_max_level_for_all_components (mongoc_structured_log_level_t level);

Sets all per-component maximum log levels to the same value.
Only log messages at or below this severity level will be passed to :symbol:`mongoc_structured_log_func_t`.
Effective even for logging components not known at compile-time.

Parameters
----------

* ``level``: The max log level for all components, as a :symbol:`mongoc_structured_log_level_t`.

.. seealso::

  | :doc:`structured_log`
