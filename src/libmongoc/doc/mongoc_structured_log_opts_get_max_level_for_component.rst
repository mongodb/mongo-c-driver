:man_page: mongoc_structured_log_opts_get_max_level_for_component

mongoc_structured_log_opts_get_max_level_for_component()
========================================================

Synopsis
--------

.. code-block:: c

  mongoc_structured_log_level_t
  mongoc_structured_log_opts_get_max_level_for_component (const mongoc_structured_log_opts_t *opts,
                                                          mongoc_structured_log_component_t component);

Parameters
----------

* ``opts``: Structured log options, allocated with :symbol:`mongoc_structured_log_opts_new`.
* ``component``: Log component as a :symbol:`mongoc_structured_log_component_t`.

Returns
-------

Returns the configured maximum log level for a specific component.
This may be the last value set with :symbol:`mongoc_structured_log_opts_set_max_level_for_component` or :symbol:`mongoc_structured_log_opts_set_max_level_for_all_components`, or it may be the default obtained from environment variables.
If an invalid or unknown component enum is given, returns the lowest log level.

.. seealso::

  | :doc:`structured_log`
