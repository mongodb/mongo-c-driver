:man_page: mongoc_uri_set_server_monitoring_mode

mongoc_uri_set_server_monitoring_mode()
=======================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_uri_set_server_monitoring_mode (mongoc_uri_t *uri, const char *value);

Parameters
----------

* ``uri``: A :symbol:`mongoc_uri_t`.
* ``value``: The new ``serverMonitoringMode`` value.

Description
-----------

Sets the ``serverMonitoringMode`` URI option to ``value`` after the URI has been parsed from a string.

Updates the option in-place if already set, otherwise appends it to the URI's :symbol:`bson:bson_t` of options.

Returns
-------

Returns false if the ``value`` is not "auto", "poll", or "stream".

