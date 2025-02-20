:man_page: bson_array_builder_append_vector_int8_elements

bson_array_builder_append_vector_int8_elements()
================================================

Synopsis
--------

.. code-block:: c

  bool
  bson_array_builder_append_vector_int8_elements (bson_array_builder_t *builder,
                                                  bson_vector_int8_const_view_t view);

Parameters
----------

* ``builder``: A valid :symbol:`bson_array_builder_t`.
* ``view``: A :symbol:`bson_vector_int8_const_view_t` pointing to validated ``int8`` :doc:`binary_vector` data.

Description
-----------

Converts the Vector pointed to by ``view`` into elements of a plain BSON Array, written to ``builder``.
Every element will be losslessly extended from ``int8_t`` to ``int32_t``.

Returns
-------

Returns ``true`` if the operation was applied successfully. The function fails if appending the array grows ``bson`` larger than INT32_MAX.

.. seealso::

  | :symbol:`bson_append_array_from_vector`
  | :symbol:`bson_array_builder_append_vector_elements`
