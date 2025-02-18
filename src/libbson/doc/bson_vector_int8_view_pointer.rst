a``:man_page: bson_vector_int8_view_pointer

bson_vector_int8_view_pointer()
===============================

Obtain a direct ``int8_t`` pointer to the Vector elements referenced by a :symbol:`bson_vector_int8_view_t`.

Synopsis
--------

.. code-block:: c

  int8_t *
  bson_vector_int8_view_pointer (bson_vector_int8_view_t view);

Parameters
----------

* ``view``: A valid :symbol:`bson_vector_int8_view_t`.

Description
-----------

Unwraps a vector view into a bare pointer.
The ``int8`` Vector elements use a serialized format that's fully compatible with a C ``int8_t``.

Returns
-------

A pointer derived from the pointer this View was created from.
Its lifetime matches that of the original pointer.
If the view was created from a :symbol:`bson_iter_t`, it will be valid until the underlying :symbol:`bson_t` is otherwise modified or destroyed.

.. seealso::

  | :symbol:`bson_vector_int8_const_view_pointer`
