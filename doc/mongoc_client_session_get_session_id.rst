:man_page: mongoc_client_session_get_session_id

mongoc_client_session_get_session_id()
======================================

Synopsis
--------

.. code-block:: c

  const bson_t *
  mongoc_client_session_get_session_id (mongoc_client_session_t *session);

Get the ID of the server-side session associated with this :symbol:`mongoc_client_session_t` as a BSON document.

Parameters
----------

* ``session``: A :symbol:`mongoc_client_session_t`.

Returns
-------

A :symbol:`bson:bson_t` you must not modify or free.

.. only:: html

  .. taglist:: See Also:
    :tags: session
