:man_page: bson_vector_float32_view_read

bson_vector_float32_view_read()
===============================

Copy a contiguous block of elements from a :symbol:`bson_vector_float32_view_t` into a C array of ``float``.

Synopsis
--------

.. code-block:: c

  bool
  bson_vector_float32_view_read (bson_vector_float32_view_t view,
                                 float *values_out,
                                 size_t element_count,
                                 size_t vector_offset_elements);

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_float32_view_t`.
* ``values_out``: Location where the ``float`` elements will be read to.
* ``element_count``: Number of elements to read.
* ``vector_offset_elements``: The vector index of the first element to read.

Description
-----------

Elements are copied in bulk from the view to the provided output pointer.

Returns
-------

If the ``element_count`` and ``vector_offset_elements`` parameters overflow the bounds of the Vector, returns false without taking any other action.
If the parameters are in range, this is guaranteed to succeed.
On success, returns true and reads ``element_count`` elements into ``*values_out``.

.. seealso::

  | :symbol:`bson_vector_float32_const_view_read`
