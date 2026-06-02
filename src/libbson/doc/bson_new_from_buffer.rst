:man_page: bson_new_from_buffer

bson_new_from_buffer()
======================

Synopsis
--------

.. code-block:: c

  bson_t *
  bson_new_from_buffer (uint8_t **buf,
                        size_t *buf_len,
                        bson_realloc_func realloc_func,
                        void *realloc_func_ctx);

Parameters
----------

* ``buf``: An out-pointer to a buffer containing a serialized BSON document, or to NULL.
* ``buf_len``: An out-pointer to the length of the buffer in bytes.
* ``realloc_func``: Optional :symbol:`bson_realloc_func` for reallocating the buffer.
* ``realloc_func_ctx``: Optional pointer that will be passed as ``ctx`` to ``realloc_func``.

Description
-----------

Creates a new :symbol:`bson_t` using the data provided.

The ``realloc_func``, if provided, is called to resize ``buf`` if the document is later expanded, for example by a call to one of the ``bson_append`` functions.

If ``*buf`` is initially NULL then it is allocated, using ``realloc_func`` or the default allocator, and initialized with an empty BSON document, and ``*buf_len`` is set to 5, the size of an empty document.

If ``*buf`` is initially non-NULL, ``*buf_len`` must be at least as large as the embedded length in the first four bytes of the BSON data in ``*buf``, but may exceed it. Remaining buffer capacity may be used to later grow the BSON document.

Returns
-------

A newly-allocated :symbol:`bson_t` on success, or NULL.

.. only:: html

  .. include:: includes/seealso/create-bson.txt
