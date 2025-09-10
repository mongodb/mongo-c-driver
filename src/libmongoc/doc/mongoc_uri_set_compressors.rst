:man_page: mongoc_uri_set_compressors

mongoc_uri_set_compressors()
============================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_uri_set_compressors (mongoc_uri_t *uri, const char *compressors);

Parameters
----------

* ``uri``: A :symbol:`mongoc_uri_t`.
* ``compressors``: A string consisting of one or more comma (,) separated
  compressors names (e.g. "snappy,zlib") or ``NULL``. Passing ``NULL`` or an
  empty string clears any existing compressors set on ``uri``.

Description
-----------

Sets the URI's compressors, after the URI has been parsed from a string.
Will overwrite any previously set value.

Example
-------

.. code-block:: c

    mongoc_client_t *client;
    mongoc_uri_t *uri;

    uri = mongoc_uri_new ("mongodb://localhost/");
    mongoc_uri_set_compressors (uri, "snappy,zlib,zstd");
    mongoc_client_new_from_uri (uri);
    /* Snappy & zlib & zstd compressors are enabled */

Returns
-------

Returns false if the option cannot be set, for example if ``compressors`` is not valid UTF-8.
Logs a warning to stderr with the :doc:`MONGOC_WARNING <logging>` macro
if compressor is not available.

