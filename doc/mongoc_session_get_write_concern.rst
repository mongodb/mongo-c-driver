:man_page: mongoc_session_get_write_concern

mongoc_session_get_write_concern()
==================================

Synopsis
--------

.. code-block:: c

  const mongoc_write_concern_t *
  mongoc_session_get_write_concern (const mongoc_session_t *session);

Retrieve the default write concern configured for the session instance. The result should not be modified or freed.

Parameters
----------

* ``session``: A :symbol:`mongoc_session_t`.

Returns
-------

A :symbol:`mongoc_write_concern_t`.

