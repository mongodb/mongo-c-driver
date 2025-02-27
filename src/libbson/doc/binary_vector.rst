:man_page: libbson_binary_vector

BSON Binary Vector subtype
==========================

In Libbson, we use the term *Vector* to refer to a data representation for compact storage of uniform elements, defined by the `BSON Binary Subtype 9 - Vector <https://github.com/mongodb/specifications/blob/master/source/bson-binary-vector/bson-binary-vector.md>`_ specification.

Libbson includes API support for Vectors:

* The *view* APIs provide an efficient way to access elements of Vector fields that reside within :symbol:`bson_t` storage.
* Integration between *views* and other Libbson features: append, array builder, iter.
* Vectors can be converted to and from a plain BSON Array, subject to the specification's type conversion rules.

The specification currently defines three element types, which Libbson interprets as:

* ``int8``: signed integer elements, equivalent to C ``int8_t``.
* ``float32``: IEEE 754 floating point, 32 bits per element, least-significant byte first. After alignment and byte swapping, elements are equivalent to C ``float``.
* ``packed_bit``: single-bit integer elements, packed most-significant bit first. Accessible in packed form as C ``uint8_t`` or as unpacked elements using C ``bool``.

Vector Views
------------

.. toctree::
  :titlesonly:
  :maxdepth: 1

  bson_vector_int8_view_t
  bson_vector_int8_const_view_t
  bson_vector_float32_view_t
  bson_vector_float32_const_view_t
  bson_vector_packed_bit_view_t
  bson_vector_packed_bit_const_view_t

Integration
-----------

* Allocating Vectors inside :symbol:`bson_t`:

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_append_vector_int8_uninit
    bson_append_vector_float32_uninit
    bson_append_vector_packed_bit_uninit

* Accessing an existing Vector via :symbol:`bson_iter_t`:

  .. code-block:: c

    #define BSON_ITER_HOLDS_VECTOR(iter) /* ... */
    #define BSON_ITER_HOLDS_VECTOR_INT8(iter) /* ... */
    #define BSON_ITER_HOLDS_VECTOR_FLOAT32(iter) /* ... */
    #define BSON_ITER_HOLDS_VECTOR_PACKED_BIT(iter) /* ... */

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_vector_int8_view_from_iter
    bson_vector_int8_const_view_from_iter
    bson_vector_float32_view_from_iter
    bson_vector_float32_const_view_from_iter
    bson_vector_packed_bit_view_from_iter
    bson_vector_packed_bit_const_view_from_iter

Array Conversion
----------------

* Polymorphic array-from-vector:

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_append_array_from_vector

* Type specific array-from-vector:

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_append_array_from_vector_int8
    bson_append_array_from_vector_float32
    bson_append_array_from_vector_packed_bit

* Using :symbol:`bson_array_builder_t` for array-from-vector:

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_array_builder_append_vector_int8_elements
    bson_array_builder_append_vector_float32_elements
    bson_array_builder_append_vector_packed_bit_elements
    bson_array_builder_append_vector_elements

* Type specific vector-from-array:

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_append_vector_int8_from_array
    bson_append_vector_float32_from_array
    bson_append_vector_packed_bit_from_array

Additional Definitions
----------------------

* Binary subtype:

  .. code-block:: c

    typedef enum {
      BSON_SUBTYPE_VECTOR = 0x09,
      /* ... */
    } bson_subtype_t;

* Byte length of the Vector header:

  .. code-block:: c

    // Length of the required header for BSON_SUBTYPE_VECTOR, in bytes
    #define BSON_VECTOR_HEADER_LEN 2

* Byte length for a Vector with specific element type and count:

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_vector_int8_binary_data_length
    bson_vector_float32_binary_data_length
    bson_vector_packed_bit_binary_data_length

* Errors:

  .. code-block:: c

    // Error "domain"
    #define BSON_ERROR_VECTOR 4

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_vector_error_code_t
