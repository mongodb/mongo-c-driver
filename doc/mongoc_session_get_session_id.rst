:man_page: mongoc_session_get_session_id

mongoc_session_get_session_id()
===============================

Synopsis
--------

.. code-block:: c

  const bson_value_t *
  mongoc_session_get_session_id (mongoc_session_t *session);

Get the ID of the server-side session associated with this :symbol:`mongoc_session_t`, or NULL if there is no associated server session.

Parameters
----------

* ``session``: A :symbol:`mongoc_session_t`.

Returns
-------

A :symbol:`bson:bson_value_t` you must not modify or free, or NULL.

.. only:: html

  .. taglist:: See Also:
    :tags: session
