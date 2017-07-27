:man_page: mongoc_session_get_read_concern

mongoc_session_get_read_concern()
=================================

Synopsis
--------

.. code-block:: c

  const mongoc_read_concern_t *
  mongoc_session_get_read_concern (const mongoc_session_t *session);

Retrieve the default read concern configured for the session instance. The result should not be modified or freed.

Parameters
----------

* ``session``: A :symbol:`mongoc_session_t`.

Returns
-------

A :symbol:`mongoc_read_concern_t`.

