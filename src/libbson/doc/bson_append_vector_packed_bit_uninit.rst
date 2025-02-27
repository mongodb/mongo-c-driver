:man_page: bson_append_vector_packed_bit_uninit

bson_append_vector_packed_bit_uninit()
===============================

Synopsis
--------

.. code-block:: c

  #define BSON_APPEND_VECTOR_PACKED_BIT_UNINIT(b, key, count, view) \
     bson_append_vector_packed_bit_uninit (b, key, (int) strlen (key), count, view)

  bool
  bson_append_vector_packed_bit_uninit (bson_t *bson,
                                        const char *key,
                                        int key_length,
                                        size_t element_count,
                                        bson_vector_packed_bit_view_t *view_out);

Parameters
----------

* ``bson``: A :symbol:`bson_t`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``element_count``: Number of elements to allocate space for.
* ``view_out``: Receives a :symbol:`bson_vector_packed_bit_view_t` with uninitialized elements.

Description
-----------

Appends a new field to ``bson`` by allocating a Vector with the indicated number of ``packed_bit`` elements.
The elements will be uninitialized.
On success, the caller must write every element in the Vector if the resulting :symbol:`bson_t` is to be used.

The view written to ``*view_out`` is only valid until ``bson`` is otherwise modified or freed.

Returns
-------

Returns ``true`` if the operation was applied successfully. The function fails if appending the array grows ``bson`` larger than INT32_MAX.
