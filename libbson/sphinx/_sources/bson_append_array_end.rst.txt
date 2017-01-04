:man_page: bson_append_array_end

bson_append_array_end()
=======================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_array_end (bson_t *bson, bson_t *child);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``child``: The :symbol:`bson_t <bson_t>` initialized in a call to :symbol:`bson_append_array_begin() <bson_append_array_begin>`.

Description
-----------

The :symbol:`bson_append_array_end() <bson_append_array_end>` function shall complete the appending of an array field started with :symbol:`bson_append_array_begin() <bson_append_array_begin>`. ``child`` is invalid after calling this function.

Returns
-------

true if there the operation could be applied without overflowing.

