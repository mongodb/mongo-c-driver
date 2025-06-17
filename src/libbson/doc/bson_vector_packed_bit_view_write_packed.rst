:man_page: bson_vector_packed_bit_view_write_packed

bson_vector_packed_bit_view_write_packed()
==========================================

Copy a contiguous block of packed bytes into a :symbol:`bson_vector_packed_bit_view_t`.

Synopsis
--------

.. code-block:: c

  bool
  bson_vector_packed_bit_view_write_packed (bson_vector_packed_bit_view_t view,
                                            const uint8_t *packed_values,
                                            size_t byte_count,
                                            size_t vector_offset_bytes);

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_packed_bit_view_t`.
* ``packed_values``: Location where the packed bytes will be copied from.
* ``byte_count``: Number of bytes to write.
* ``vector_offset_bytes``: The byte index of the first packed byte to write.

Description
-----------

Packed bytes are copied in bulk from the input pointer to the provided view.

If the Vector's element count isn't a multiple of 8, its final byte will include bits that do not belong to any element.
This function cannot be used to modify the unused bits, they will be explicitly zeroed if set.

Returns
-------

If the ``byte_count`` and ``vector_offset_bytes`` parameters overflow the bounds of the Vector, returns false without taking any other action.
If the parameters are in range, this is guaranteed to succeed.
On success, returns true and writes ``byte_count`` bytes from ``*packed_values`` into the Vector starting at byte index ``vector_offset_bytes``.

.. seealso::

  | :symbol:`bson_vector_packed_bit_view_read_packed`
  | :symbol:`bson_vector_packed_bit_const_view_read_packed`
