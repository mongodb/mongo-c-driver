:man_page: bson_vector_float32_const_view_length

bson_vector_float32_const_view_length()
=======================================

Return the number of elements in a Vector referenced by a :symbol:`bson_vector_float32_const_view_t`.

Synopsis
--------

.. code-block:: c

  size_t
  bson_vector_float32_const_view_length (bson_vector_float32_const_view_t view);

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_float32_const_view_t`.

Description
-----------

An element count is calculated from the view's stored binary block length.

Returns
-------

The number of elements, as a ``size_t``.

.. seealso::

  | :symbol:`bson_vector_float32_view_length`
