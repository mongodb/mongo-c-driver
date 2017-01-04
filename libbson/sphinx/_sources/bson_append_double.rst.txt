:man_page: bson_append_double

bson_append_double()
====================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_double (bson_t *bson,
                      const char *key,
                      int key_length,
                      double value);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``value``: A double value to append.

Description
-----------

The :symbol:`bson_append_double() <bson_append_double>` function shall append a new element to a bson document of type ``double``.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

