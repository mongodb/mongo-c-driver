:man_page: mongoc_structured_log_get_named_level

mongoc_structured_log_get_named_level
=====================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_structured_log_get_named_level (const char *name, mongoc_structured_log_level_t *out);

Look up a log level by name. Case insensitive.

Parameters
----------

* ``name``: A name to look up as a log level.
* ``out``: On success, the corresponding :symbol:`mongoc_structured_log_level_t` is written here.

Returns
-------

If the level name is known, returns ``true`` and writes the level enum to ``*out``.
If the level name is not known, returns ``false`` and does not write ``*out``.

.. seealso::

  | :doc:`structured_log`
