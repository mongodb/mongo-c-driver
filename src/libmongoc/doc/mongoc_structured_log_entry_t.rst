:man_page: mongoc_structured_log_entry_t

mongoc_structured_log_entry_t
=============================

Synopsis
--------

.. code-block:: c

  typedef struct mongoc_structured_log_entry_t mongoc_structured_log_entry_t;

``mongoc_structured_log_entry_t`` is an opaque structure which represents the temporary state of an in-progress log entry.
It can only be used during a :symbol:`mongoc_structured_log_func_t`, it is not valid after the log handler returns.
Use the functions below to query individual aspects of the log entry.

Functions
---------

.. toctree::
  :titlesonly:
  :maxdepth: 1

  mongoc_structured_log_entry_get_component
  mongoc_structured_log_entry_get_level
  mongoc_structured_log_entry_get_message_string
  mongoc_structured_log_entry_message_as_bson

.. seealso::

  | :doc:`structured_log`
