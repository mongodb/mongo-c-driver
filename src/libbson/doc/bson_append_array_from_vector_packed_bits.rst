:man_page: bson_append_array_from_vector_packed_bits

bson_append_array_from_vector_packed_bits()
===========================================

Synopsis
--------

.. code-block:: c

  #define BSON_APPEND_ARRAY_FROM_VECTOR_PACKED_BITS(b, key, view) \
    bson_append_array_from_vector_packed_bits (b, key, (int) strlen (key), view)

  bool
  bson_append_array_from_vector_packed_bits (bson_t *bson,
                                             const char *key,
                                             int key_length,
                                             bson_vector_packed_bits_const_view_t view);

Parameters
----------

* ``bson``: A :symbol:`bson_t`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``view``: A :symbol:`bson_vector_packed_bits_const_view_t` pointing to validated ``packed_bits`` :doc:`binary_vector` data.

Description
-----------

Converts the Vector pointed to by ``view`` into a plain BSON Array, written to ``bson`` under the name ``key``.

Returns
-------

Returns ``true`` if the operation was applied successfully. The function fails if appending the array grows ``bson`` larger than INT32_MAX.

.. seealso::

  | :symbol:`bson_array_builder_append_vector_packed_bits_elements`
