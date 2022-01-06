:man_page: mongoc_uri_get_service

mongoc_uri_get_service()
========================

Synopsis
--------

.. code-block:: c

  const char *
  mongoc_uri_get_service (const mongoc_uri_t *uri)
     BSON_GNUC_DEPRECATED_FOR (mongoc_uri_get_srv_hostname);

Parameters
----------

* ``uri``: A :symbol:`mongoc_uri_t`.

Deprecated
----------

This function is deprecated and should not be used in new code.

Please use :symbol:`mongoc_uri_get_srv_hostname()` instead.

Returns the SRV host and domain name of a MongoDB URI.

Returns
-------

A string if this URI's scheme is "mongodb+srv://", or NULL if the scheme is "mongodb://".
