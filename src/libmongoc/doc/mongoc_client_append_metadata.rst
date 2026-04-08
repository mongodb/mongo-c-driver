:man_page: mongoc_client_append_metadata

mongoc_client_append_metadata()
===============================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_client_append_metadata (mongoc_client_t *client,
                                 const char *name,
                                 const char *version,
                                 const char *platform)

Append metadata to the handshake command sent as part of the initial connection handshake (`"hello" <https://www.mongodb.com/docs/manual/reference/command/hello/>`_).

See :symbol:`mongoc_handshake_data_append()` for more details.

The updated handshake command applies only to the given ``client`` object for connections established **after** the append took place.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``name``: The name of the wrapping driver. Must not be null or an empty string.
* ``version``: The optional version of the wrapping driver.
* ``platform``: The optional information about the current platform, for example configure options or compile flags.

No string argument may contain the substring " / ", which is used as the delimiter between metadata field values.

Returns
-------

This function will log an error and return ``false`` when one of the following occurs:

* ``client`` is from a :symbol:`mongoc_client_pool_t`: use :symbol:`mongoc_client_pool_append_metadata()` instead.
* The resulting handshake document would exceed the size limit.

Otherwise, ``true`` if the given fields are set successfully.

.. seealso::

  | :symbol:`mongoc_client_pool_append_metadata()`

  | :symbol:`mongoc_handshake_data_append()`
