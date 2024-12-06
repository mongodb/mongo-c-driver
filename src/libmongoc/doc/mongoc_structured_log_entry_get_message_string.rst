:man_page: mongoc_structured_log_entry_get_message_string

mongoc_structured_log_entry_get_message_string()
================================================

Synopsis
--------

.. code-block:: c

  const char *
  mongoc_structured_log_entry_get_message_string (const mongoc_structured_log_entry_t *entry);

Parameters
----------

* ``entry``: A :symbol:`mongoc_structured_log_entry_t` pointer.

Returns
-------

A string, guaranteed to be valid only during the lifetime of the structured log handler.
It should not be freed or modified.

Identical to the value of the ``message`` key in the document returned by :symbol:`mongoc_structured_log_entry_message_as_bson`.

This is not a complete string representation of the structured log, but rather a standardized identifier for a particular log event.

.. seealso::

  | :doc:`structured_log`
