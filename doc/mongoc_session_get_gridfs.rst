:man_page: mongoc_session_get_gridfs

mongoc_session_get_gridfs()
===========================

Synopsis
--------

.. code-block:: c

  mongoc_gridfs_t *
  mongoc_session_get_gridfs (mongoc_session_t *session,
                             const char *db,
                             const char *prefix,
                             bson_error_t *error);

Create a GridFS handle that is bound to a session.

Parameters
----------

* ``session``: A :symbol:`mongoc_session_t`.
* ``db``: The database name.
* ``prefix``: Optional prefix for gridfs collection names or ``NULL``.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

On success, returns a :symbol:`mongoc_gridfs_t` you must free with :symbol:`mongoc_gridfs_destroy()` before destroying the session.. Returns ``NULL`` upon failure and sets ``error``.

.. only:: html

  .. taglist:: See Also:
    :tags: session
