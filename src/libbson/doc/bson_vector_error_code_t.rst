:man_page: bson_vector_error_code_t

bson_vector_error_code_t
========================

BSON Error codes for :doc:`binary_vector` operations that could fail in multiple ways.

Synopsis
--------

.. code-block:: c

  #define BSON_ERROR_VECTOR 4

  typedef enum {
    BSON_VECTOR_ERROR_ARRAY_ELEMENT_TYPE = 1,
    BSON_VECTOR_ERROR_ARRAY_ELEMENT_VALUE = 2,
    BSON_VECTOR_ERROR_ARRAY_KEY = 3,
    BSON_VECTOR_ERROR_MAX_SIZE = 4,
  } bson_vector_error_code_t;

Description
-----------

The error ``code`` values in ``bson_vector_error_code_t`` apply to :symbol:`bson_error_t` values with a ``category`` of ``BSON_ERROR_CATEGORY`` and a ``domain`` of ``BSON_ERROR_VECTOR``.

* ``BSON_VECTOR_ERROR_ARRAY_ELEMENT_TYPE``: An element was encountered with incorrect type. Location and type details in ``message``.
* ``BSON_VECTOR_ERROR_ARRAY_ELEMENT_VALUE``: An element was encountered with out-of-range value. Location and value details in ``message``.
* ``BSON_VECTOR_ERROR_ARRAY_KEY``: An input BSON Array did not contain the expected numeric key value. Expected and actual keys in ``message``.
* ``BSON_VECTOR_ERROR_MAX_SIZE``: The BSON maximum document size would be exceeded. Equivalent to a failure from ``bson_append_*`` functions that do not return an ``error``.
