:man_page: bson_vector_int8_view_as_const

bson_vector_int8_view_as_const()
================================

Convert a :symbol:`bson_vector_int8_view_t` into a :symbol:`bson_vector_int8_const_view_t`.

Synopsis
--------

.. code-block:: c

  bson_vector_int8_const_view_t
  bson_vector_int8_view_as_const (bson_vector_int8_view_t view);

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_int8_view_t`.

Description
-----------

This adds a ``const`` qualifier to the view without re-validating the underlying data.

Returns
-------

Always returns a :symbol:`bson_vector_int8_const_view_t`.
