:man_page: bson_append_array_unsafe_begin

bson_append_array_unsafe_begin()
================================

Synopsis
--------

.. code-block:: c

  #define BSON_APPEND_ARRAY_UNSAFE_BEGIN(b, key, child) \
     bson_append_array_unsafe_begin (b, key, (int) strlen (key), child)

  bool
  bson_append_array_unsafe_begin (bson_t *bson,
                                  const char *key,
                                  int key_length,
                                  bson_t *child);

Parameters
----------

* ``bson``: A :symbol:`bson_t`.
* ``key``: A string containing the name for the key.
* ``key_length``: The length of ``key`` or -1 to call ``strlen()``.
* ``child``: A :symbol:`bson_t`.

Description
-----------

The :symbol:`bson_append_array_unsafe_begin()` function shall begin appending an array field to ``bson``. This allows for incrementally building a sub-array. Doing so will generally yield better performance as you will serialize to a single buffer. When done building the sub-array, the caller *MUST* call :symbol:`bson_append_array_end()`.

The caller is responsible for generating array element keys correctly ("0", "1", "2", ...). For generating array element keys, see :symbol:`bson_uint32_to_string`.

Consider using :symbol:`bson_array_builder_t` to append an array without needing to generate array element keys.

Returns
-------

Returns ``true`` if the operation was applied successfully. The function will fail if appending the array grows ``bson`` larger than INT32_MAX.

.. seealso::

  | :symbol:`bson_array_builder_t`
