:man_page: mongoc_client_pool_append_metadata

mongoc_client_pool_append_metadata()
====================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_client_pool_append_metadata (mongoc_client_pool_t *pool,
                                      const char *name,
                                      const char *version,
                                      const char *platform)

This function is identical to :symbol:`mongoc_client_append_metadata()` except for client pools.

See :symbol:`mongoc_client_append_metadata()` and :symbol:`mongoc_handshake_data_append()` for more details.

The updated handshake command applies to the given ``pool`` object for connections established **after** the append took place.

Also note that :symbol:`mongoc_client_append_metadata()` cannot be called on a client retrieved from a client pool.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.
* ``name``: The name of the wrapping driver. Must not be null or an empty string.
* ``version``: The optional version of the wrapping driver.
* ``platform``: The optional information about the current platform, for example configure options or compile flags.

No string argument may contain the substring " / ", which is used as the delimiter between metadata field values.

Returns
-------

This function will log an error and return ``false`` when one of the following occurs:

* The resulting handshake document would exceed the size limit.

Otherwise, ``true`` if the given fields are set successfully.

.. include:: includes/mongoc_client_pool_thread_safe.txt

.. seealso::

  | :symbol:`mongoc_client_append_metadata()`

  | :symbol:`mongoc_handshake_data_append()`
