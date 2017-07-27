:man_page: mongoc_session_get_read_prefs

mongoc_session_get_read_prefs()
===============================

Synopsis
--------

.. code-block:: c

  const mongoc_read_prefs_t *
  mongoc_session_get_read_prefs (const mongoc_session_t *session);

Retrieves the default read preferences configured for the session instance. The result should not be modified or freed.

Parameters
----------

* ``session``: A :symbol:`mongoc_session_t`.

Returns
-------

A :symbol:`mongoc_read_prefs_t`.

