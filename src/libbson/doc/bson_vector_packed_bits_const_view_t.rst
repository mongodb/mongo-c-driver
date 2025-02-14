:man_page: bson_vector_packed_bits_const_view_t

bson_vector_packed_bits_const_view_t
====================================

A reference to non-owned const BSON Binary data holding a valid Vector of ``packed_bits`` element type.

Synopsis
--------

.. code-block:: c

  #include <bson/bson.h>

  typedef struct bson_vector_packed_bits_const_view_t {
     /*< private >*/
  } bson_vector_packed_bits_const_view_t;

Description
-----------

:symbol:`bson_vector_packed_bits_const_view_t` is a structure that acts as an opaque const reference to a block of memory that has been validated as a ``packed_bits`` vector.

It is meant to be passed by value and can be discarded at any time. The contents of the structure should be considered private.

The :symbol:`bson_t` *MUST* be valid for the lifetime of the view and it is an error to modify the :symbol:`bson_t` while using the view.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_vector_packed_bits_const_view_init
    bson_vector_packed_bits_const_view_from_iter
    bson_vector_packed_bits_const_view_length
    bson_vector_packed_bits_const_view_length_bytes
    bson_vector_packed_bits_const_view_padding
    bson_vector_packed_bits_const_view_read_packed
    bson_vector_packed_bits_const_view_unpack_bool

Example
-------

.. code-block:: c

  bson_iter_t iter;
  bson_vector_packed_bits_const_view_t view;

  if (bson_iter_init_find (&iter, &doc, "vector") && bson_vector_packed_bits_const_view_from_iter (&view, &iter)) {
    size_t length = bson_vector_packed_bits_const_view_length (view);
    size_t length_bytes = bson_vector_packed_bits_const_view_length_bytes (view);
    size_t padding = bson_vector_packed_bits_const_view_padding (view);

    printf ("Elements in 'vector':\n");
    for (size_t i = 0; i < length; i++) {
      bool element;
      BSON_ASSERT (bson_vector_packed_bits_const_view_unpack_bool (view, &element, 1, i));
      printf (" elements[%d] = %d\n", (int) i, (int) element);
    }

    printf ("Bytes in 'vector': (%d bits unused)\n", (int) padding);
    for (size_t i = 0; i < length_bytes; i++) {
      uint8_t packed_byte;
      BSON_ASSERT (bson_vector_packed_bits_const_view_read_packed (view, &packed_byte, 1, i));
      printf (" bytes[%d] = 0x%02x\n", (int) i, (unsigned) packed_byte);
    }
  }

.. seealso::

  | :symbol:`bson_vector_packed_bits_view_t`
