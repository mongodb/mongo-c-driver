:man_page: bson_vector_packed_bit_const_view_read_packed

bson_vector_packed_bit_const_view_read_packed()
===============================================

Copy a contiguous block of packed bytes out of a :symbol:`bson_vector_packed_bit_const_view_t`.

Synopsis
--------

.. code-block:: c

  bool
  bson_vector_packed_bit_const_view_read_packed (bson_vector_packed_bit_const_view_t view,
                                                 uint8_t *packed_values_out,
                                                 size_t byte_count,
                                                 size_t vector_offset_bytes);

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_packed_bit_const_view_t`.
* ``packed_values_out``: Location where the packed bytes will be read to.
* ``byte_count``: Number of bytes to read.
* ``vector_offset_bytes``: The byte index of the first packed byte to read.

Description
-----------

Packed bytes are copied in bulk from the view to the provided output pointer.

If the Vector's element count isn't a multiple of 8, its final byte will include bits that do not belong to any element.
Vector validation checks that these bits are zero.

Returns
-------

If the ``byte_count`` and ``vector_offset_bytes`` parameters overflow the bounds of the Vector, returns false without taking any other action.
If the parameters are in range, this is guaranteed to succeed.
On success, returns true and reads ``byte_count`` bytes into ``*packed_values_out``.

.. seealso::

  | :symbol:`bson_vector_packed_bit_view_read_packed`
  | :symbol:`bson_vector_packed_bit_view_write_packed`
