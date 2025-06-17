:man_page: bson_vector_packed_bit_const_view_length_bytes

bson_vector_packed_bit_const_view_length_bytes()
================================================

Return the number of packed bytes in a Vector referenced by a :symbol:`bson_vector_packed_bit_const_view_t`.

Synopsis
--------

.. code-block:: c

  size_t
  bson_vector_packed_bit_const_view_length_bytes (bson_vector_packed_bit_const_view_t view);

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_packed_bit_const_view_t`.

Description
-----------

A byte count is calculated from the view's stored binary block length.
If the element count isn't a multiple of 8, the final byte will include bits that do not belong to any element.

Returns
-------

The number of bytes, as a ``size_t``.

.. seealso::

  | :symbol:`bson_vector_packed_bit_view_length_bytes`
