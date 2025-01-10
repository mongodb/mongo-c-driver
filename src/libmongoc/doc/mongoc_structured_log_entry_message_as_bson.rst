:man_page: mongoc_structured_log_entry_message_as_bson

mongoc_structured_log_entry_message_as_bson()
=============================================

Synopsis
--------

.. code-block:: c

  bson_t *
  mongoc_structured_log_entry_message_as_bson (const mongoc_structured_log_entry_t *entry);

Make a new copy, as a :symbol:`bson_t`, of the log entry's standardized BSON representation.
When possible, a log handler should avoid serializing log messages that will be discarded.
Each call allocates an independent copy of the message that must be freed.

Parameters
----------

* ``entry``: A :symbol:`mongoc_structured_log_entry_t` pointer.

Returns
-------

A new allocated :symbol:`bson_t` that must be freed with a call to :symbol:`bson_destroy`.

.. seealso::

  | :doc:`structured_log`
