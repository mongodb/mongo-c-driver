:man_page: bson_append_symbol

bson_append_symbol()
====================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_symbol (bson_t *bson,
                      const char *key,
                      int key_length,
                      const char *value,
                      int length);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``value``: The symbol.
* ``length``: A length of ``symbol`` in bytes, or -1 to determine the length with ``strlen()``.

Description
-----------

Appends a new field to ``bson`` of type BSON_TYPE_SYMBOL. This BSON type is deprecated and should not be used in new code.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

