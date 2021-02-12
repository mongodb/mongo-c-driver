:man_page: mongoc_collection_set_timeout_ms

mongoc_collection_set_timeout_ms()
==================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_collection_set_timeout_ms (mongoc_collection_t *collection, int64_t timeout_ms, bson_error_t *error)

Sets the client-side operation timeout value for ``collection`` to the given ``timeout_ms``. If there is an error setting the timeout, this method will return false and ``error`` will be set.


This function will set ``error`` and return false in the following cases:

* ``timeout_ms`` is a negative number

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``timeout_ms``: A value, in milliseconds, for the timeout. Must be non-negative.

Returns
-------

true if the timeout_ms is set successfully. Otherwise, false.
