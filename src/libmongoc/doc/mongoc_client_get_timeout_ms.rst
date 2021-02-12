:man_page: mongoc_client_get_timeout_ms

mongoc_client_get_timeout_ms()
==============================

Synopsis
--------

.. code-block:: c

  int64_t
  mongoc_client_get_timeout_ms (const mongoc_client_t *client)

Returns the client-side operation timeout value for this object. If ``client`` does not have a timeout currently set, this method returns -1 for MONGOC_TIMEOUTMS_UNSET.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.

Returns
-------

The timeout set on this client or -1 for MONGOC_TIMEOUTMS_UNSET.
