:man_page: bson_vector_packed_bit_view_t

bson_vector_packed_bit_view_t
=============================

A reference to mutable non-owned BSON Binary data holding a valid Vector of ``packed_bit`` element type.

Synopsis
--------

.. code-block:: c

  #include <bson/bson.h>

  typedef struct bson_vector_packed_bit_view_t {
     /*< private >*/
  } bson_vector_packed_bit_view_t;

Description
-----------

:symbol:`bson_vector_packed_bit_view_t` is a structure that acts as an opaque reference to a block of memory that has been validated as a ``packed_bit`` vector.

It is meant to be passed by value and can be discarded at any time. The contents of the structure should be considered private.

The :symbol:`bson_t` *MUST* be valid for the lifetime of the view and it is an error to modify the :symbol:`bson_t` while using the view.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_vector_packed_bit_view_init
    bson_vector_packed_bit_view_from_iter
    bson_vector_packed_bit_view_as_const
    bson_vector_packed_bit_view_length
    bson_vector_packed_bit_view_length_bytes
    bson_vector_packed_bit_view_padding
    bson_vector_packed_bit_view_read_packed
    bson_vector_packed_bit_view_write_packed
    bson_vector_packed_bit_view_unpack_bool
    bson_vector_packed_bit_view_pack_bool

Example
-------

.. code-block:: c

  // Fill a new vector with individual boolean elements
  {
      static const bool bool_values[] = {true, false, true, true, false};
      const size_t bool_values_count = sizeof bool_values / sizeof bool_values[0];

      bson_vector_packed_bit_view_t view;
      BSON_ASSERT (BSON_APPEND_VECTOR_PACKED_BIT_UNINIT (&doc, "from_bool", bool_values_count, &view));
      BSON_ASSERT (bson_vector_packed_bit_view_pack_bool (view, bool_values, bool_values_count, 0));
  }

  // Fill another new vector with packed bytes
  {
      static const uint8_t packed_bytes[] = {0xb0};
      const size_t unused_bits_count = 3;
      const size_t packed_values_count = sizeof packed_bytes * 8 - unused_bits_count;

      bson_vector_packed_bit_view_t view;
      BSON_ASSERT (BSON_APPEND_VECTOR_PACKED_BIT_UNINIT (&doc, "from_packed", packed_values_count, &view));
      BSON_ASSERT (bson_vector_packed_bit_view_write_packed (view, packed_bytes, sizeof packed_bytes, 0));
  }

  // Compare both vectors. They match exactly.
  {
      bson_iter_t from_bool_iter, from_packed_iter;
      BSON_ASSERT (bson_iter_init_find (&from_bool_iter, &doc, "from_bool"));
      BSON_ASSERT (bson_iter_init_find (&from_packed_iter, &doc, "from_packed"));
      BSON_ASSERT (bson_iter_binary_equal (&from_bool_iter, &from_packed_iter));
  }

.. seealso::

  | :symbol:`bson_append_vector_packed_bit_uninit`
  | :symbol:`bson_vector_packed_bit_const_view_t`
