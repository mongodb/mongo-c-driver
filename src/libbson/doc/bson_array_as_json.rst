:man_page: bson_array_as_json

bson_array_as_json()
====================

Synopsis
--------

.. code-block:: c

  char *
  bson_array_as_json (const bson_t *bson, size_t *length);

Parameters
----------

* ``bson``: A :symbol:`bson_t`.
* ``length``: An optional location for the length of the resulting string.

Description
-----------

The :symbol:`bson_array_as_json()` function shall encode ``bson`` as a UTF-8
string using libbson's legacy JSON format, except the outermost element is
encoded as a JSON array, rather than a JSON document. This function is
superseded by :symbol:`bson_array_as_canonical_extended_json()` and
:symbol:`bson_array_as_relaxed_extended_json()`, which use the same 
`MongoDB Extended JSON format`_ as all other MongoDB drivers.
The caller is responsible for freeing the resulting UTF-8 encoded string by
calling :symbol:`bson_free()` with the result.

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
     /* BSON array is a normal BSON document with integer values for the keys,
      * starting with 0 and continuing sequentially
      */
     BSON_APPEND_UTF8 (&bson, "0", "foo");
     BSON_APPEND_UTF8 (&bson, "1", "bar");

     str = bson_array_as_json (&bson, NULL);
     /* Prints
      * [ "foo", "bar" ]
      */
     printf ("%s\n", str);
     bson_free (str);

     bson_destroy (&bson);
  }


.. only:: html

  .. include:: includes/seealso/bson-as-json.txt

.. _MongoDB Extended JSON format: https://github.com/mongodb/specifications/blob/master/source/extended-json/extended-json.md
