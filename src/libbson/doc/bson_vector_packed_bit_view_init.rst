:man_page: bson_vector_packed_bit_view_init

bson_vector_packed_bit_view_init()
==================================

Initialize a :symbol:`bson_vector_packed_bit_view_t` from a mutable ``uint8_t`` buffer.

Synopsis
--------

.. code-block:: c

  bool
  bson_vector_packed_bit_view_init (bson_vector_packed_bit_view_t *view_out,
                                    uint8_t *binary_data,
                                    uint32_t binary_data_len);

Parameters
----------

* ``view_out``: A :symbol:`bson_vector_packed_bit_view_t` is written here on success.
* ``binary_data``: A pointer to the BSON Binary data block to be validated.
* ``binary_data_len``: Length of the binary data block, in bytes.

Description
-----------

The length, header, and trailing padding of the provided binary data block will be checked for a valid Vector of ``packed_bit`` element type.
On success, the pointer and length are packaged as a :symbol:`bson_vector_packed_bit_view_t` written to ``*view_out``.
The view will only be valid as long as ``binary_data`` is valid.

Returns
-------

Returns true if the view was successfully initialized.
