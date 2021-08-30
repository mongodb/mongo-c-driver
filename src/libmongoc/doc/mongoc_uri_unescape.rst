:man_page: mongoc_uri_unescape

mongoc_uri_unescape()
=====================

Synopsis
--------

.. code-block:: c

  char *
  mongoc_uri_unescape (const char *escaped_string) BSON_GNUC_WARN_UNUSED_RESULT;

Parameters
----------

* ``escaped_string``: A utf8 encoded string.

Description
-----------

Unescapes an URI encoded string. For example, "%40" would become "@".

Returns
-------

Returns a newly allocated string that should be freed with :symbol:`bson:bson_free()`. If ``escaped_string`` contains an invalid UTF-8 character or an invalid escape sequence, returns ``NULL``.

