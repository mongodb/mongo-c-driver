:man_page: mongoc_structured_log_entry_get_level

mongoc_structured_log_entry_get_level
=====================================

Synopsis
--------

.. code-block:: c

  mongoc_structured_log_level_t
  mongoc_structured_log_entry_get_level (const mongoc_structured_log_entry_t *entry);

Parameters
----------

* ``entry``: A :symbol:`mongoc_structured_log_entry_t` pointer.

Returns
-------

The :symbol:`mongoc_structured_log_level_t` associated with this log entry.

.. seealso::

  | :doc:`structured_log`
