:man_page: bson_append_document_end

bson_append_document_end()
==========================

Synopsis
--------

.. code-block:: c

  bool
  bson_append_document_end (bson_t *bson, bson_t *child);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``child``: The child :symbol:`bson_t <bson_t>` initialized in a call to :symbol:`bson_append_document_begin() <bson_append_document_begin>`.

Description
-----------

The :symbol:`bson_append_document_end() <bson_append_document_end>` function shall complete the appending of a document with :symbol:`bson_append_document_begin() <bson_append_document_begin>`. ``child`` is invalid after calling this function.

Returns
-------

true if the operation was applied successfully, otherwise false and ``bson`` should be discarded.

