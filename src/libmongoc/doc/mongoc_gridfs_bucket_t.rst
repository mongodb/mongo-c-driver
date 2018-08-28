:man_page: mongoc_gridfs_bucket_t

mongoc_gridfs_bucket_t
======================

Synopsis
--------

.. code-block:: c

  #include <mongoc/mongoc.h>

  typedef struct _mongoc_gridfs_bucket_t mongoc_gridfs_bucket_t;

Description
-----------

``mongoc_gridfs_bucket_t`` provides a MongoDB GridFS implementation, superseding :symbol:`mongoc_gridfs_t`. See the `GridFS MongoDB documentation <https://docs.mongodb.com/manual/core/gridfs/>`_.

Thread Safety
-------------

``mongoc_gridfs_bucket_t`` is NOT thread-safe and should only be used in the same thread as the owning :symbol:`mongoc_client_t`.

Example
-------

.. literalinclude:: ../examples/example-gridfs-bucket.c
   :language: c
   :caption: example-gridfs-bucket.c


.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_gridfs_bucket_abort_upload
    mongoc_gridfs_bucket_delete_by_id
    mongoc_gridfs_bucket_destroy
    mongoc_gridfs_bucket_download_to_stream
    mongoc_gridfs_bucket_find
    mongoc_gridfs_bucket_new
    mongoc_gridfs_bucket_open_download_stream
    mongoc_gridfs_bucket_open_upload_stream
    mongoc_gridfs_bucket_open_upload_stream_with_id
    mongoc_gridfs_bucket_stream_error
    mongoc_gridfs_bucket_upload_from_stream
    mongoc_gridfs_bucket_upload_from_stream_with_id