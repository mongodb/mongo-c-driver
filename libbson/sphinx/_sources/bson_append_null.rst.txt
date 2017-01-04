:man_page: bson_append_null

bson_append_null()
==================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_null (bson_t *bson, const char *key, int key_length);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.

Description
-----------

The :symbol:`bson_append_null() <bson_append_null>` function shall append a new element to ``bson`` of type BSON_TYPE_NULL.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

