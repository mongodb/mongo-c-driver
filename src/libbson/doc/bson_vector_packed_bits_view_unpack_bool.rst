:man_page: bson_vector_packed_bits_view_unpack_bool

bson_vector_packed_bits_view_unpack_bool()
================================================

Unpack a contiguous block of elements from a :symbol:`bson_vector_packed_bits_view_t` into a C array of ``bool``.

Synopsis
--------

.. code-block:: c

  bool
  bson_vector_packed_bits_view_unpack_bool (bson_vector_packed_bits_view_t view,
                                            bool *unpacked_values_out,
                                            size_t element_count,
                                            size_t vector_offset_elements);

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_packed_bits_view_t`.
* ``unpacked_values_out``: Location where the ``bool`` elements will be unpacked to.
* ``element_count``: Number of elements to unpack.
* ``vector_offset_elements``: The vector index of the first element to unpack.

Description
-----------

Elements are unpacked from individual bits into a C array of ``bool``.

Returns
-------

If the ``element_count`` and ``vector_offset_elements`` parameters overflow the bounds of the Vector, returns false without taking any other action.
If the parameters are in range, this is guaranteed to succeed.
On success, returns true and unpacks ``element_count`` elements into ``*unpacked_values_out``.

.. seealso::

  | :symbol:`bson_vector_packed_bits_const_view_unpack_bool`
