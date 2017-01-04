:man_page: bson_append_oid

bson_append_oid()
=================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_oid (bson_t *bson,
                   const char *key,
                   int key_length,
                   const bson_oid_t *oid);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``oid``: A bson_oid_t.

Description
-----------

The :symbol:`bson_append_oid() <bson_append_oid>` function shall append a new element to ``bson`` of type BSON_TYPE_OID. ``oid`` *MUST* be a pointer to a :symbol:`bson_oid_t <bson_oid_t>`.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

