:man_page: mongoc_change_stream_next

mongoc_change_stream_next()
===========================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_change_stream_next (mongoc_change_stream_t *stream,
                             const bson_t **bson);

This function iterates the underlying cursor, setting ``bson`` to the next
document. This will block for a maximum of ``maxAwaitTimeMS`` milliseconds as
specified in the options when created, or the default timeout if omitted. Data
may be returned before the timeout. If no data is returned this function returns
``false``.

Parameters
----------

* ``stream``: A :symbol:`mongoc_change_stream_t` obtained from :symbol:`mongoc_collection_watch`.
* ``bson``: The location for the resulting document.

Returns
-------
A boolean indicating whether or not there was another document in the stream.

Similar to :symbol:`mongoc_cursor_next` the lifetime of ``bson`` is until the
next call to :symbol:`mongoc_change_stream_next`, so it needs to be copied to
extend the lifetime.
