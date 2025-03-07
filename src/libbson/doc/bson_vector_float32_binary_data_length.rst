:man_page: bson_vector_float32_binary_data_length

bson_vector_float32_binary_data_length()
========================================

Calculate the size of a BSON Binary field that would be needed to store a Vector with the indicated number of ``float32`` elements.

Synopsis
--------

.. code-block:: c

  uint32_t
  bson_vector_float32_binary_data_length (size_t element_count);

Parameters
----------

* ``element_count``: Number of elements, as a ``size_t``.

Description
-----------

Checks ``element_count`` against the maximum representable size, and calculates the required Binary size.

Returns
-------

On success, returns the required Binary size as a ``uint32_t`` greater than or equal to ``BSON_VECTOR_HEADER_LEN``.
This length includes the 2-byte Vector header, but not the Binary subtype header or any other BSON headers.
If the ``element_count`` is too large to represent, returns 0.
