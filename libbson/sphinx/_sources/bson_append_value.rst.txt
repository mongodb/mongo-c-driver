:man_page: bson_append_value

bson_append_value()
===================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_value (bson_t *bson,
                     const char *key,
                     int key_length,
                     const bson_value_t *value);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``value``: A :symbol:`bson_value_t <bson_value_t>`.

Description
-----------

Appends a new field to ``bson`` by determining the boxed type in ``value``. This is useful if you want to copy fields between documents but do not know the field type until runtime.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

