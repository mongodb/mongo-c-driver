:man_page: bson_vector_packed_bits_view_pack_bool

bson_vector_packed_bits_view_pack_bool()
========================================

Pack a contiguous block of elements from a C ``bool`` array into a :symbol:`bson_vector_packed_bits_view_t`.

Synopsis
--------

.. code-block:: c

  bool
  bson_vector_packed_bits_view_pack_bool (bson_vector_packed_bits_view_t view,
                                          const bool *unpacked_values,
                                          size_t element_count,
                                          size_t vector_offset_elements)

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_packed_bits_view_t`.
* ``unpacked_values``: Location where the ``bool`` elements will be packed from.
* ``element_count``: Number of elements to pack.
* ``vector_offset_elements``: The vector index of the first element to pack.

Description
-----------

Elements are packed into individual Vector bits from a C ``bool`` array.

Returns
-------

If the ``element_count`` and ``vector_offset_elements`` parameters overflow the bounds of the Vector, returns false without taking any other action.
If the parameters are in range, this is guaranteed to succeed.
On success, returns true and writes to ``element_count`` elements in the Vector starting at ``vector_offset_elements``.
