:man_page: bson_append_bool

bson_append_bool()
==================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_bool (bson_t *bson, const char *key, int key_length, bool value);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: The name of the field.
* ``key_length``: The length of ``key`` or -1 to use strlen().
* ``value``: true or false.

Description
-----------

The :symbol:`bson_append_bool() <bson_append_bool>` function shall append a new element to ``bson`` containing the boolean provided.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

