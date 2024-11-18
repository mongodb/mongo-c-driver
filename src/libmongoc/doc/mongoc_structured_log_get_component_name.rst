:man_page: mongoc_structured_log_get_component_name

mongoc_structured_log_get_component_name
========================================

Synopsis
--------

.. code-block:: c

  const char *
  mongoc_structured_log_get_component_name (mongoc_structured_log_component_t component);

Parameters
----------

* ``component``: Log component as a :symbol:`mongoc_structured_log_component_t`.

Returns
-------

If the component is known, returns a pointer to a constant string that should not be freed.
If the component has no known name, returns NULL.

.. seealso::

  | :doc:`structured_log`
