:man_page: bson_append_binary

bson_append_binary()
====================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_binary (bson_t *bson,
                      const char *key,
                      int key_length,
                      bson_subtype_t subtype,
                      const uint8_t *binary,
                      uint32_t length);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: The key name.
* ``key_length``: The length of ``key`` in bytes or -1 to use strlen().
* ``subtype``: A bson_subtype_t indicationg the binary subtype.
* ``binary``: A buffer to embed as binary data.
* ``length``: The length of ``buffer`` in bytes.

Description
-----------

The :symbol:`bson_append_binary() <bson_append_binary>` function shall append a new element to ``bson`` containing the binary data provided.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

