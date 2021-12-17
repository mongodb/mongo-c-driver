:man_page: mongoc_uri_get_srv_hostname

mongoc_uri_get_srv_hostname()
====================================

Synopsis
--------

.. code-block:: c

  MONGOC_EXPORT (const char *)
  mongoc_uri_get_srv_hostname (const mongoc_uri_t *uri)
     BSON_GNUC_DEPRECATED_FOR (mongoc_uri_get_srv_hostname);

Parameters
----------

* ``uri``: A :symbol:`mongoc_uri_t`.

Description
-----------

Returns the SRV host and domain name of a MongoDB URI.

Returns
-------

A string if this URI's scheme is "mongodb+srv://", or NULL if the scheme is "mongodb://".
