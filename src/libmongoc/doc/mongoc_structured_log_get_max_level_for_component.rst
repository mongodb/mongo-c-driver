:man_page: mongoc_structured_log_get_max_level_for_component

mongoc_structured_log_get_max_level_for_component
=================================================

Synopsis
--------

.. code-block:: c

  mongoc_structured_log_level_t
  mongoc_structured_log_get_max_level_for_component (mongoc_structured_log_component_t component);

Parameters
----------

* ``component``: Log component as a :symbol:`mongoc_structured_log_component_t`.

Returns
-------

Returns the current maximum log level for a specific component.
This may be the last value set with :symbol:`mongoc_structured_log_set_max_level_for_component` or :symbol:`mongoc_structured_log_set_max_level_for_all_components`, or it may be the default obtained from environment variables.

.. seealso::

  | :doc:`structured_log`
