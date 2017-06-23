:man_page: mongoc_database_get_session

mongoc_database_get_session()
=============================

Synopsis
--------

.. code-block:: c

  const mongoc_session_t *
  mongoc_database_get_session (mongoc_database_t *database);

Parameters
----------

* ``database``: A :symbol:`mongoc_database_t`.

Description
-----------

Fetches the session to which this database is bound, if any.

This function only retrieves an existing session handle, to actually create
a session use :symbol:`mongoc_client_start_session`.

Returns
-------

A const :symbol:`mongoc_session_t` or NULL.
