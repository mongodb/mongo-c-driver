:man_page: bson_append_document_begin

bson_append_document_begin()
============================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_document_begin (bson_t *bson,
                              const char *key,
                              int key_length,
                              bson_t *child);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``key``: An ASCII C string containing the name of the field.
* ``key_length``: The length of ``key`` in bytes, or -1 to determine the length with ``strlen()``.
* ``child``: An uninitialized :symbol:`bson_t <bson_t>` to be initialized as the sub-document.

Description
-----------

The :symbol:`bson_append_document_begin() <bson_append_document_begin>` function shall begin appending a sub-document to ``bson``. Use ``child`` to add fields to the sub-document. When completed, call :symbol:`bson_append_document_end() <bson_append_document_end>` to complete the element.

``child`` *MUST* be an uninitialized :symbol:`bson_t <bson_t>` to avoid leaking memory.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

