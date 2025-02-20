:man_page: bson_array_builder_append_vector_elements

bson_array_builder_append_vector_elements()
===========================================

Synopsis
--------

.. code-block:: c

  bool
  bson_array_builder_append_vector_elements (bson_array_builder_t *builder,
                                             const bson_iter_t *iter);

Parameters
----------

* ``builder``: A valid :symbol:`bson_array_builder_t`.
* ``iter``: A :symbol:`bson_iter_t` pointing to any supported :doc:`binary_vector` field.

Description
-----------

Converts the Vector pointed to by ``iter`` into elements of a plain BSON Array, written to ``builder``.
This conversion is polymorphic: A converted element type will be chosen based on the type of the input Vector.
For details, see the type-specific versions of this function.

Returns
-------

Returns ``true`` if the operation was applied successfully. The function fails if appending the array grows ``bson`` larger than INT32_MAX, or if ``iter`` doesn't point to a valid recognized Vector type.

.. seealso::

  | :symbol:`bson_append_array_from_vector`
  | :symbol:`bson_array_builder_append_vector_int8_elements`
  | :symbol:`bson_array_builder_append_vector_float32_elements`
  | :symbol:`bson_array_builder_append_vector_packed_bits_elements`
