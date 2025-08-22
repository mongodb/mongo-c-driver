:man_page: mongoc_stream_buffered_new

mongoc_stream_buffered_new()
============================

Synopsis
--------

.. code-block:: c

  mongoc_stream_t *
  mongoc_stream_buffered_new (mongoc_stream_t *base_stream, size_t buffer_size);

Parameters
----------

* ``base_stream``: A :symbol:`mongoc_stream_t` to buffer.
* ``buffer_size``: A size_t containing the desired buffer size.

This function shall create a new :symbol:`mongoc_stream_t` that buffers bytes to and from the underlying ``base_stream``.

``buffer_size`` will be used as the initial buffer size. It may grow past this size.

.. warning::
  
  The internal buffer does not reduce in size once grown. Receiving a large message may result in a large allocation that persists until the returned :symbol:`mongoc_stream_t` is freed with :symbol:`mongoc_stream_destroy()`.

Returns
-------

A newly allocated :symbol:`mongoc_stream_buffered_t` on success, otherwise ``NULL``. This should be freed with :symbol:`mongoc_stream_destroy()` when no longer in use.

