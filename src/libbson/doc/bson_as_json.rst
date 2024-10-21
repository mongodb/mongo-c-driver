:man_page: bson_as_json

bson_as_json()
==============

.. warning::
   .. deprecated:: 1.29.0

      This function is deprecated and should not be used in new code.

Synopsis
--------

.. code-block:: c

  char *
  bson_as_json (const bson_t *bson, size_t *length) BSON_GNUC_DEPRECATED_FOR (bson_as_legacy_extended_json);

Parameters
----------

* ``bson``: A :symbol:`bson_t`.
* ``length``: An optional location for the length of the resulting string.

Description
-----------

The :symbol:`bson_as_json()` function shall encode ``bson`` as a UTF-8 string using :doc:`libbson's Legacy Extended JSON <legacy_extended_json>`.
This function is superseded by :symbol:`bson_as_canonical_extended_json()` and :symbol:`bson_as_relaxed_extended_json()`, which use the same `MongoDB Extended JSON format`_ as all other MongoDB drivers.
To continue producing Legacy Extended JSON, :symbol:`bson_as_legacy_extended_json()` may be used.

The caller is responsible for freeing the resulting UTF-8 encoded string by calling :symbol:`bson_free()` with the result.

If non-NULL, ``length`` will be set to the length of the result in bytes.

Returns
-------

If successful, a newly allocated UTF-8 encoded string and ``length`` is set.

Upon failure, NULL is returned.

Example
-------

.. code-block:: c

  #include <bson/bson.h>

  int main ()
  {
     bson_t bson;
     char *str;

     bson_init (&bson);
     BSON_APPEND_UTF8 (&bson, "0", "foo");
     BSON_APPEND_UTF8 (&bson, "1", "bar");

     str = bson_as_json (&bson, NULL);
     /* Prints
      * { "0" : "foo", "1" : "bar" }
      */
     printf ("%s\n", str);
     bson_free (str);

     bson_destroy (&bson);
  }

.. only:: html

  .. include:: includes/seealso/bson-as-json.txt

.. _MongoDB Extended JSON format: https://github.com/mongodb/specifications/blob/master/source/extended-json/extended-json.md
