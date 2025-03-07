:man_page: bson_vector_float32_view_t

bson_vector_float32_view_t
==========================

A reference to mutable non-owned BSON Binary data holding a valid Vector of ``float32`` element type.

Synopsis
--------

.. code-block:: c

  #include <bson/bson.h>

  typedef struct bson_vector_float32_view_t {
     /*< private >*/
  } bson_vector_float32_view_t;

Description
-----------

:symbol:`bson_vector_float32_view_t` is a structure that acts as an opaque reference to a block of memory that has been validated as a ``float32`` vector.

It is meant to be passed by value and can be discarded at any time. The contents of the structure should be considered private.

The :symbol:`bson_t` *MUST* be valid for the lifetime of the view and it is an error to modify the :symbol:`bson_t` while using the view.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_vector_float32_view_init
    bson_vector_float32_view_from_iter
    bson_vector_float32_view_as_const
    bson_vector_float32_view_length
    bson_vector_float32_view_read
    bson_vector_float32_view_write

Example
-------

.. code-block:: c

  static const float values[] = {1.0f, 2.0f, 3.0f};
  const size_t values_count = sizeof values / sizeof values[0];

  bson_vector_float32_view_t view;
  BSON_ASSERT (BSON_APPEND_VECTOR_FLOAT32_UNINIT (&doc, "vector", values_count, &view));
  BSON_ASSERT (bson_vector_float32_view_write (view, values, values_count, 0));

.. seealso::

  | :symbol:`bson_append_vector_float32_uninit`
  | :symbol:`bson_vector_float32_const_view_t`
