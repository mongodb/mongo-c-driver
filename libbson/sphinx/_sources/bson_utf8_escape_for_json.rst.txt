:man_page: bson_utf8_escape_for_json

bson_utf8_escape_for_json()
===========================

Synopsis
--------

.. code-block:: c

  char *
  bson_utf8_escape_for_json (const char *utf8, ssize_t utf8_len);

Parameters
----------

* ``utf8``: A UTF-8 encoded string.
* ``utf8_len``: The length of ``utf8`` in bytes or -1 if it is NULL terminated.

Description
-----------

Escapes the string ``utf8`` to be placed inside of a JSON string.

Returns
-------

A newly allocated string that should be freed with :symbol:`bson_free() <bson_free>`.

