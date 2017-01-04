:man_page: bson_as_json

bson_as_json()
==============

Synopsis
--------

.. code-block:: c

  char *
  bson_as_json (const bson_t *bson, size_t *length);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``length``: An optional location for the length of the resulting string.

Description
-----------

The :symbol:`bson_as_json() <bson_as_json>` function shall encode ``bson`` as a JSON encoded UTF-8 string.

The caller is responsible for freeing the resulting UTF-8 encoded string by calling :symbol:`bson_free() <bson_free>` with the result.

If non-NULL, ``length`` will be set to the length of the result in bytes.

Returns
-------

If successful, a newly allocated UTF-8 encoded string and ``length`` is set.

Upon failure, NULL is returned.

Example
-------

.. code-block:: c

  char *str = bson_as_json (doc, NULL);
  printf ("%s\n", str);
  bson_free (str);

