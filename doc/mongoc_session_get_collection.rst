:man_page: mongoc_session_get_collection

mongoc_session_get_collection()
===============================

Synopsis
--------

.. code-block:: c

  mongoc_collection_t *
  mongoc_session_get_collection (const mongoc_session_t *session,
                                 const char *db,
                                 const char *collection);

Create a collection handle that is bound to a session.

See the example code for :symbol:`mongoc_session_t`.

Parameters
----------

* ``session``: A :symbol:`mongoc_session_t`.
* ``db``: A database name.
* ``collection``: A collection name.

Returns
-------

A :symbol:`mongoc_collection_t` you must free with :symbol:`mongoc_collection_destroy()` before destroying the session.

.. only:: html

  .. taglist:: See Also:
    :tags: session
