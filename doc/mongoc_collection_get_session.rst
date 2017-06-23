:man_page: mongoc_collection_get_session

mongoc_collection_get_session()
===============================

Synopsis
--------

.. code-block:: c

  const mongoc_session_t *
  mongoc_collection_get_session (mongoc_collection_t *collection);

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.

Description
-----------

Fetches the session to which this collection is bound, if any.

This function only retrieves an existing session handle, to actually create
a session use :symbol:`mongoc_client_start_session`.

Returns
-------

A const :symbol:`mongoc_session_t` or NULL.
