:man_page: mongoc_structured_log_func_t

mongoc_structured_log_func_t
============================

Synopsis
--------

.. code-block:: c

  typedef void (*mongoc_structured_log_func_t)
  (const mongoc_structured_log_entry_t *entry, void *user_data);

Callback function for :symbol:`mongoc_structured_log_set_handler`.
Structured log handlers must be thread-safe.
Logging may occur simultaneously on multiple application threads and/or background threads.

Handlers may use any operating system or ``libbson`` functions but MUST not use any ``libmongoc`` functions except:

* :symbol:`mongoc_structured_log_entry_get_component`
* :symbol:`mongoc_structured_log_entry_get_level`
* :symbol:`mongoc_structured_log_entry_message_as_bson`
* :symbol:`mongoc_structured_log_get_level_name`
* :symbol:`mongoc_structured_log_get_named_level`
* :symbol:`mongoc_structured_log_get_component_name`
* :symbol:`mongoc_structured_log_get_named_component`
* :symbol:`mongoc_structured_log_get_max_level_for_component`

Use of other ``libmongoc`` APIs will have undefined consequences, including deadlocks and unbounded recursion.

Applications that wish to use MongoDB itself as a logging destination will need to store the serialized messages temporarily and insert them asynchronously from outside the log handler.

Parameters
----------

* ``entry``: A :symbol:`mongoc_structured_log_entry_t` pointer, only valid during the handler invocation.
* ``user_data``: Optional user data from :symbol:`mongoc_structured_log_set_handler`.

.. seealso::

  | :doc:`structured_log`
