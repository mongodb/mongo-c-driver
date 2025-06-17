:man_page: bson_vector_int8_view_write

bson_vector_int8_view_write()
=============================

Copy a contiguous block of elements from a C ``int8_t`` array into a :symbol:`bson_vector_int8_view_t`.

Synopsis
--------

.. code-block:: c

  bool
  bson_vector_int8_view_write (bson_vector_int8_view_t view,
                               const int8_t *values,
                               size_t element_count,
                               size_t vector_offset_elements);

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_int8_view_t`.
* ``values``: Location where the ``int8_t`` elements will be copied from.
* ``element_count``: Number of elements to write.
* ``vector_offset_elements``: The vector index of the first element to write.

Description
-----------

Elements are copied in bulk from the provided pointer into the view.

Returns
-------

If the ``element_count`` and ``vector_offset_elements`` parameters overflow the bounds of the Vector, returns false without taking any other action.
If the parameters are in range, this is guaranteed to succeed.
On success, returns true and writes to ``element_count`` elements in the Vector starting at ``vector_offset_elements``.
