:man_page: mongoc_apm_command_succeeded_get_database_name

mongoc_apm_command_succeeded_get_database_name()
================================================

Synopsis
--------

.. code-block:: c

  const char *
  mongoc_apm_command_succeeded_get_database_name (
     const mongoc_apm_command_succeeded_t *event);

Returns this event's database name. The data is only valid in the scope of the callback that receives this event; copy it if it will be accessed after the callback returns.

Parameters
----------

* ``event``: A :symbol:`mongoc_apm_command_succeeded_t`.

Returns
-------

A string that should not be modified or freed.

.. seealso::

  | :doc:`Introduction to Application Performance Monitoring <application-performance-monitoring>`

