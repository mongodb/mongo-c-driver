:man_page: mongoc_uri_get_service

mongoc_uri_get_service()
========================

.. warning::
   .. deprecated:: 1.21.0

      Use :symbol:`mongoc_uri_get_srv_hostname()` instead.

Synopsis
--------

.. code-block:: c

  const char *
  mongoc_uri_get_service (const mongoc_uri_t *uri);

Parameters
----------

* ``uri``: A :symbol:`mongoc_uri_t`.

Returns the SRV host and domain name of a MongoDB URI.

Returns
-------

A string if this URI's scheme is "mongodb+srv://", or NULL if the scheme is "mongodb://".
