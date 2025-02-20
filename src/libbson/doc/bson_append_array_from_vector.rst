:man_page: bson_append_array_from_vector

bson_append_array_from_vector()
===============================

Synopsis
--------

.. code-block:: c

  #define BSON_APPEND_ARRAY_FROM_VECTOR(b, key, iter) \
    bson_append_array_from_vector (b, key, (int) strlen (key), iter)

  bool
  bson_append_array_from_vector (bson_t *bson,
                                 const char *key,
                                 int key_length,
                                 const bson_iter_t *iter);

Parameters
----------

* ``bson``: A :symbol:`bson_t`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``iter``: A :symbol:`bson_iter_t` pointing to any supported :doc:`binary_vector` field.

Description
-----------

Converts the Vector pointed to by ``iter`` into a plain BSON Array, written to ``bson`` under the name ``key``.

Returns
-------

Returns ``true`` if the operation was applied successfully. The function fails if appending the array grows ``bson`` larger than INT32_MAX, or if ``iter`` doesn't point to a valid recognized Vector type.

.. seealso::

  | :symbol:`bson_array_builder_append_vector_elements`
