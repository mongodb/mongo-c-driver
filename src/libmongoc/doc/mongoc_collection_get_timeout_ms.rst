:man_page: mongoc_collection_get_timeout_ms

mongoc_collection_get_timeout_ms()
==================================

Synopsis
--------

.. code-block:: c

  int64_t
  mongoc_collection_get_timeout_ms (const mongoc_collection_t *collection)

Returns the client-side operation timeout value for this object. If ``collection`` does not have a timeout currently set, this method will return a value inherited from the parent :symbol:`mongoc_database_t` or :symbol:`mongoc_client_t` object. If no timeout is set on any parent object, returns -1 for MONGOC_TIMEOUTMS_UNSET.

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.

Returns
-------

The timeout set on this collection or inherited from a parent object.
