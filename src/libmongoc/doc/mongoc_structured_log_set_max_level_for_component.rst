:man_page: mongoc_structured_log_set_max_level_for_component

mongoc_structured_log_set_max_level_for_component
=================================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_structured_log_set_max_level_for_component (mongoc_structured_log_component_t component,
                                                     mongoc_structured_log_level_t level);

Sets the maximum log level per-component.
Only log messages at or below this severity level will be passed to :symbol:`mongoc_structured_log_func_t`.

By default, each component's log level comes from the environment variables ``MONGOC_LOG_<component>`` and ``MONGOC_LOG_ALL``.

Parameters
----------

* ``component``: The component to set a max log level. for, as a :symbol:`mongoc_structured_log_component_t`.
* ``level``: The new max log level for this component, as a :symbol:`mongoc_structured_log_level_t`.

.. seealso::

  | :doc:`structured_log`
