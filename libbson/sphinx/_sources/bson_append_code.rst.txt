:man_page: bson_append_code

bson_append_code()
==================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_code (bson_t *bson,
                    const char *key,
                    int key_length,
                    const char *javascript);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: The key name.
* ``key_length``: The length of ``key`` or -1 to use strlen().
* ``javascript``: A UTF-8 encoded string containing the javascript.

Description
-----------

The :symbol:`bson_append_code() <bson_append_code>` function shall append a new element to ``bson`` using the UTF-8 encoded ``javascript`` provided. ``javascript`` must be a NULL terminated C string.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

