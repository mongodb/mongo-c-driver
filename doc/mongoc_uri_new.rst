:man_page: mongoc_uri_new

mongoc_uri_new()
================

Synopsis
--------

.. code-block:: c

  mongoc_uri_t *
  mongoc_uri_new (const char *uri_string) BSON_GNUC_WARN_UNUSED_RESULT;

Parameters
----------

* ``uri_string``: A string containing a URI.

Description
-----------

Parses a string containing a MongoDB style URI connection string.

Returns
-------

A newly allocated :symbol:`mongoc_uri_t` if successful. Otherwise ``NULL``.

.. warning::

  Failure to handle the result of this function is a programming error.

Examples
--------

Examples of some valid MongoDB connection strings can be seen below.

``"mongodb://localhost/"``

``"mongodb://localhost/?replicaSet=myreplset"``

``"mongodb://myuser:mypass@localhost/"``

``"mongodb://kerberosuser%40EXAMPLE.COM@example.com/?authMechanism=GSSAPI"``

``"mongodb://[::1]:27017/"``

``"mongodb://10.0.0.1:27017,10.0.0.1:27018,[::1]:27019/?ssl=true"``

``"mongodb://%2Ftmp%2Fmongodb-27017.sock"``

``"mongodb://user:pass@%2Ftmp%2Fmongodb-27017.sock"``

``"mongodb://localhost,[::1]/mydb?authSource=mydb"``

