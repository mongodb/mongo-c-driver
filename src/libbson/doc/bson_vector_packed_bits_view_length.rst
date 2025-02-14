:man_page: bson_vector_packed_bits_view_length

bson_vector_packed_bits_view_length()
=====================================

Return the number of elements in a Vector referenced by a :symbol:`bson_vector_packed_bits_view_t`.

Synopsis
--------

.. code-block:: c

  size_t
  bson_vector_packed_bits_view_length (bson_vector_packed_bits_view_t view);

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_packed_bits_view_t`.

Description
-----------

An element count is calculated from information stored inside the `bson_vector_packed_bits_view_t` value.

Returns
-------

The number of elements, as a ``size_t``.
