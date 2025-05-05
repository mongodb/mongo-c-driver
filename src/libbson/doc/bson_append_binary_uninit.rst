:man_page: bson_append_binary_uninit

bson_append_binary_uninit()
===========================

Synopsis
--------

.. code-block:: c

  #define BSON_APPEND_BINARY_UNINIT(b, key, subtype, val, len) \
     bson_append_binary_uninit (b, key, (int) strlen (key), subtype, val, len)

  bool
  bson_append_binary_uninit (bson_t *bson,
                             const char *key,
                             int key_length,
                             bson_subtype_t subtype,
                             uint8_t **binary,
                             uint32_t length);

Parameters
----------

* ``bson``: A :symbol:`bson_t`.
* ``key``: The key name.
* ``key_length``: The length of ``key`` in bytes or -1 to use strlen().
* ``subtype``: A bson_subtype_t indicating the binary subtype.
* ``binary``: Location for a pointer that will receive a writable pointer to the uninitialized binary data block.
* ``length``: The length of ``buffer`` in bytes.

Description
-----------

The :symbol:`bson_append_binary_uninit()` function is an alternative to :symbol:`bson_append_binary()` which allows applications to assemble the contents of the binary field within the :symbol:`bson_t`, without an additional temporary buffer.
On success, the caller MUST write to every byte of the binary data block if the resulting :symbol:`bson_t` is to be used.
The buffer that ``binary`` points to is only valid until the iterator's :symbol:`bson_t` is otherwise modified or freed.


Returns
-------

Returns ``true`` if the uninitialized ``binary`` item was appended. The function will fail if appending ``binary`` grows ``bson`` larger than INT32_MAX.
