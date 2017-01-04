:man_page: bson_append_code_with_scope

bson_append_code_with_scope()
=============================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_code_with_scope (bson_t *bson,
                               const char *key,
                               int key_length,
                               const char *javascript,
                               const bson_t *scope);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``javascript``: A NULL-terminated UTF-8 encoded string containing the javascript fragment.
* ``scope``: Optional :symbol:`bson_t <bson_t>` containing the scope for ``javascript``.

Description
-----------

The :symbol:`bson_append_code_with_scope() <bson_append_code_with_scope>` function shall perform like :symbol:`bson_append_code() <bson_append_code>` except it allows providing a scope to the javascript function in the form of a bson document.

If ``scope`` is NULL, this function appends an element with BSON type "code", otherwise with BSON type "code with scope".

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

