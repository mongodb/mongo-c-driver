:man_page: mongoc_session_t
:tags: session

mongoc_session_t
================

Use a session for a sequence of operations that require special guarantees, such as retryable writes or causally consistent reads.

Synopsis
--------

.. include:: includes/session-lifecycle.txt

Example
-------

.. literalinclude:: ../examples/example-session.c
   :language: c
   :caption: example-session.c


.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_session_get_client
    mongoc_session_get_collection
    mongoc_session_get_database
    mongoc_session_get_gridfs
    mongoc_session_get_opts
    mongoc_session_get_read_concern
    mongoc_session_get_read_prefs
    mongoc_session_get_session_id
    mongoc_session_get_write_concern
    mongoc_session_read_command_with_opts
    mongoc_session_read_write_command_with_opts
    mongoc_session_write_command_with_opts
    mongoc_session_destroy
