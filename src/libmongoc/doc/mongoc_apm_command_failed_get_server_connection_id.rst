:man_page: mongoc_apm_command_failed_get_server_connection_id

mongoc_apm_command_failed_get_server_connection_id()
====================================================

Synopsis
--------

.. code-block:: c

  void *
  mongoc_apm_command_failed_get_server_connection_id (
     const mongoc_apm_command_failed_t *event);

Returns this event's context.

Parameters
----------

* ``event``: A :symbol:`mongoc_apm_command_failed_t`.

Returns
-------

The pointer passed with :symbol:`mongoc_client_set_apm_callbacks` or :symbol:`mongoc_client_pool_set_apm_callbacks`.

.. seealso::

  | :doc:`Introduction to Application Performance Monitoring <application-performance-monitoring>`

