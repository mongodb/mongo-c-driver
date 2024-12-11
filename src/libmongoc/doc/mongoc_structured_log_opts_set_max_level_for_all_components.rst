:man_page: mongoc_structured_log_opts_set_max_level_for_all_components

mongoc_structured_log_opts_set_max_level_for_all_components()
=============================================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_structured_log_opts_set_max_level_for_all_components (mongoc_structured_log_opts_t *opts,
                                                               mongoc_structured_log_level_t level);

Sets all per-component maximum log levels to the same value.
Only log messages at or below this severity level will be passed to :symbol:`mongoc_structured_log_func_t`.
Effective even for logging components not known at compile-time.

Parameters
----------

* ``opts``: Structured log options, allocated with :symbol:`mongoc_structured_log_opts_new`.
* ``level``: The max log level for all components, as a :symbol:`mongoc_structured_log_level_t`.

Returns
-------

Returns ``true`` on success, or ``false`` if the supplied parameters were incorrect.

.. seealso::

  | :doc:`structured_log`
