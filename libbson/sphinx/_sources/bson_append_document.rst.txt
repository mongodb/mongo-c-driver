:man_page: bson_append_document

bson_append_document()
======================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_document (bson_t *bson,
                        const char *key,
                        int key_length,
                        const bson_t *value);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``value``: A :symbol:`bson_t <bson_t>` containing the sub-document to append.

Description
-----------

The :symbol:`bson_append_document() <bson_append_document>` function shall append ``child`` to ``bson`` using the specified key. The type of the field will be a document.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

