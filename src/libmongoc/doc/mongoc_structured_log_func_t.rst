:man_page: mongoc_structured_log_func_t

mongoc_structured_log_func_t
============================

Synopsis
--------

.. code-block:: c

  typedef void (*mongoc_structured_log_func_t)
  (const mongoc_structured_log_entry_t *entry, void *user_data);

Callback function for :symbol:`mongoc_structured_log_set_handler`.
Not required to be re-entrant, mutual exclusion is provided by the logging facility.

Handlers may use any operating system or ``libbson`` functions but MUST not use any ``libmongoc`` functions except:

* :symbol:`mongoc_structured_log_entry_get_component`
* :symbol:`mongoc_structured_log_entry_get_level`
* :symbol:`mongoc_structured_log_entry_message_as_bson`

Parameters
----------

* ``entry``: A :symbol:`mongoc_structured_log_entry_t` pointer, only valid during the handler invocation.
* ``user_data``: Optional user data from :symbol:`mongoc_structured_log_set_handler`.

.. seealso::

  | :doc:`structured_log`
