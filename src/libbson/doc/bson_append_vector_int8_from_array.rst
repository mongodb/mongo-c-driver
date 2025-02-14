:man_page: bson_append_vector_int8_from_array

bson_append_vector_int8_from_array()
====================================

Synopsis
--------

.. code-block:: c

  #define BSON_APPEND_VECTOR_INT8_FROM_ARRAY(b, key, iter, err) \
    bson_append_vector_int8_from_array (b, key, (int) strlen (key), iter, err)

  bool
  bson_append_vector_int8_from_array (bson_t *bson,
                                      const char *key,
                                      int key_length,
                                      const bson_iter_t *iter,
                                      bson_error_t *error);

Parameters
----------

* ``bson``: A :symbol:`bson_t`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``iter``: A :symbol:`bson_iter_t` referencing array elements that will be converted.
* ``error``: Optional :symbol:`bson_error_t` for detail about conversion failures.

Description
-----------

Appends a new field to ``bson`` by converting an Array to a Vector of ``int8`` elements.

For the conversion to succeed, every item in the Array must be an integer (``BSON_TYPE_INT32`` or ``BSON_TYPE_INT64``) in the range ``INT8_MIN`` (-128) through ``INT8_MAX`` (127) inclusive.
If any element has an incorrect type or an out-of-range value, the conversion fails with an ``error`` message providing details, and no changes are made to ``bson``.

The provided ``iter`` must be positioned just prior to the first element of the BSON Array.
If your input is a bare BSON Array, set up ``iter`` using :symbol:`bson_iter_init`.
If the input is within a document field, use :symbol:`bson_iter_recurse`.

Returns
-------

Returns ``true`` if the operation was applied successfully. On error, returns ``false`` and writes additional error information to ``error`` without modifying ``bson``.
The error will have a ``domain`` of ``BSON_ERROR_VECTOR`` and a ``code`` from :symbol:`bson_vector_error_code_t`.
