:man_page: bson_vector_int8_const_view_t

bson_vector_int8_const_view_t
=============================

A reference to non-owned const BSON Binary data holding a valid Vector of ``int8`` element type.

Synopsis
--------

.. code-block:: c

  #include <bson/bson.h>

  typedef struct bson_vector_int8_const_view_t {
     /*< private >*/
  } bson_vector_int8_const_view_t;

Description
-----------

:symbol:`bson_vector_int8_const_view_t` is a structure that acts as an opaque const reference to a block of memory that has been validated as an ``int8`` vector.

It is meant to be passed by value and can be discarded at any time. The contents of the structure should be considered private.

The :symbol:`bson_t` *MUST* be valid for the lifetime of the view and it is an error to modify the :symbol:`bson_t` while using the view.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_vector_int8_const_view_init
    bson_vector_int8_const_view_from_iter
    bson_vector_int8_const_view_length
    bson_vector_int8_const_view_read
    bson_vector_int8_const_view_pointer

Example
-------

.. code-block:: c

  bson_iter_t iter;
  bson_vector_int8_const_view_t view;

  if (bson_iter_init_find (&iter, &doc, "vector") && bson_vector_int8_const_view_from_iter (&view, &iter)) {
    size_t length = bson_vector_int8_const_view_length (view);
    printf ("Elements in 'vector':\n");
    for (size_t i = 0; i < length; i++) {
      int8_t element;
      BSON_ASSERT (bson_vector_int8_const_view_read (view, &element, 1, i));
      printf (" [%d] = %d\n", (int) i, (int) element);
    }
  }

.. seealso::

  | :symbol:`bson_vector_int8_view_t`
