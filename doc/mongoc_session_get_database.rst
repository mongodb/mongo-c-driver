:man_page: mongoc_session_get_database

mongoc_session_get_database()
=============================

Synopsis
--------

.. code-block:: c

  mongoc_database_t *
  mongoc_session_get_database (const mongoc_session_t *session,
                               const char *name);

Create a database handle that is bound to a session.

See the example code for :symbol:`mongoc_session_t`.

Parameters
----------

* ``session``: A :symbol:`mongoc_session_t`.
* ``name``: A database name.

Returns
-------

A :symbol:`mongoc_database_t` you must free with :symbol:`mongoc_database_destroy()` before destroying the session.

.. only:: html

  .. taglist:: See Also:
    :tags: session
