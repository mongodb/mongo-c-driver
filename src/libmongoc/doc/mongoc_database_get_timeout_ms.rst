:man_page: mongoc_database_get_timeout_ms

mongoc_database_get_timeout_ms()
================================

Synopsis
--------

.. code-block:: c

  int64_t
  mongoc_database_get_timeout_ms (const mongoc_database_t *database)

Returns the client-side operation timeout value for this object. If ``database`` does not have a timeout currently set, this method will return a value inherited from the parent :symbol:`mongoc_client_t` object. If no timeout is set on the parent object, returns -1, or MONGOC_TIMEOUTMS_UNSET.

Parameters
----------

* ``database``: A :symbol:`mongoc_database_t`.

Returns
-------

The timeout set on this database or inherited from a parent object.
