:man_page: mongoc_gridfs_drop

mongoc_gridfs_drop()
====================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_gridfs_drop (mongoc_gridfs_t *gridfs, bson_error_t *error);

Parameters
----------

* ``gridfs``: A :symbol:`mongoc_gridfs_t`.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Description
-----------

Requests that an entire GridFS be dropped, including all files associated with it.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

Returns true if successful, otherwise false and error is set.

