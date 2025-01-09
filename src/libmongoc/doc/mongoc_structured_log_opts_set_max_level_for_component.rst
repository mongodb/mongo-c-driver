:man_page: mongoc_structured_log_opts_set_max_level_for_component

mongoc_structured_log_opts_set_max_level_for_component()
========================================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_structured_log_opts_set_max_level_for_component (mongoc_structured_log_opts_t *opts,
                                                          mongoc_structured_log_component_t component,
                                                          mongoc_structured_log_level_t level);

Sets the maximum log level per-component.
Only log messages at or below this severity level will be passed to :symbol:`mongoc_structured_log_func_t`.

By default, each component's log level may come from environment variables.
See :symbol:`mongoc_structured_log_opts_set_max_levels_from_env`.

Parameters
----------

* ``opts``: Structured log options, allocated with :symbol:`mongoc_structured_log_opts_new`.
* ``component``: The component to set a max log level. for, as a :symbol:`mongoc_structured_log_component_t`.
* ``level``: The new max log level for this component, as a :symbol:`mongoc_structured_log_level_t`.

Returns
-------

Returns ``true`` on success, or ``false`` if the supplied parameters were incorrect.

.. seealso::

  | :doc:`structured_log`
