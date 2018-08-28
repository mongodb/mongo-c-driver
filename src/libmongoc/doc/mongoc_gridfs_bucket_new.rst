:man_page: mongoc_gridfs_bucket_new

mongoc_gridfs_bucket_new()
==========================

Synopsis
--------

.. code-block:: c

  mongoc_gridfs_bucket_t *
  mongoc_gridfs_bucket_new (mongoc_database_t *db,
                            const bson_t *opts,
                            const mongoc_read_prefs_t *read_prefs);

Parameters
----------

* ``db``: A :symbol:`mongoc_database_t`.
* ``opts``: A :symbol:`bson_t` or ``NULL``
* ``read_prefs``: A :symbol:`mongoc_read_prefs_t` used for read operations or ``NULL`` to inherit read preferences from ``db``.

``opts`` may be ``NULL`` or a document consisting of any of the following:

* ``bucketName`` A ``utf8`` string used as the prefix to the GridFS "chunks" and "files" collections. Defaults to "fs".
* ``chunkSizeBytes`` An ``int32`` representing the chunk size. Defaults to 255KB.
* ``writeConcern`` A serialized :symbol:`mongoc_write_concern_t` appended with :symbol:`mongoc_write_concern_append`. Defaults to the write concern of ``db``.
* ``readConcern`` A serialized :symbol:`mongoc_read_concern_t` appended with :symbol:`mongoc_read_concern_append`. Defaults to the read concern of ``db``.

Description
-----------

Creates a new :symbol:`mongoc_gridfs_bucket_t`. Use this handle to perform GridFS operations.

Returns
-------

A newly allocated :symbol:`mongoc_gridfs_bucket_t` that should be freed with :symbol:`mongoc_gridfs_bucket_destroy()`.
