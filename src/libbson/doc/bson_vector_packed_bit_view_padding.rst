:man_page: bson_vector_packed_bit_view_padding

bson_vector_packed_bit_view_padding()
=====================================

Returns the number of unused bits in a Vector referenced by a :symbol:`bson_vector_packed_bit_view_t`.

Synopsis
--------

.. code-block:: c

  size_t
  bson_vector_packed_bit_view_padding (bson_vector_packed_bit_view_t view);

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_packed_bit_view_t`.

Description
-----------

The 3-bit ``padding`` field is extracted from a copy of the Vector header inside ``view``.

Returns
-------

The number of unused bits in the final packed byte. Guaranteed to be between 0 and 7 inclusive.
Vector validation guarantees that empty Vectors have a ``padding`` of 0.

.. seealso::

  | :symbol:`bson_vector_packed_bit_const_view_padding`
