:man_page: bson_append_int64

bson_append_int64()
===================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_int64 (bson_t *bson,
                     const char *key,
                     int key_length,
                     int64_t value);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``value``: An int64_t.

Description
-----------

The :symbol:`bson_append_int64() <bson_append_int64>` function shall append a new element to ``bson`` containing a 64-bit signed integer.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

