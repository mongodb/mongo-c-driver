:man_page: mongoc_structured_log_get_level_name

mongoc_structured_log_get_level_name()
======================================

Synopsis
--------

.. code-block:: c

  const char *
  mongoc_structured_log_get_level_name (mongoc_structured_log_level_t level);

Parameters
----------

* ``level``: Log level as a :symbol:`mongoc_structured_log_level_t`.

Returns
-------

If the level is known, returns a pointer to a constant string that should not be freed.
If the level has no known name, returns NULL.

.. seealso::

  | :doc:`structured_log`
