:man_page: mongoc_uri_get_server_monitoring_mode

mongoc_uri_get_server_monitoring_mode()
=======================================

Synopsis
--------

.. code-block:: c

  const char *
  mongoc_uri_get_server_monitoring_mode (const mongoc_uri_t *uri);

Parameters
----------

* ``uri``: A :symbol:`mongoc_uri_t`.

Description
-----------

Fetches the ``serverMonitoringMode`` parameter to an URI if provided.

Returns
-------

A string which should not be modified or freed. Returns "auto" if the ``serverMonitoringMode`` parameter was not provided to ``uri``.

