:man_page: mongoc_client_pool_set_appname

mongoc_client_pool_set_appname()
================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_client_pool_set_appname (mongoc_client_pool_t *pool, const char *appname)

This function is identical to :symbol:`mongoc_client_set_appname() <mongoc_client_set_appname>` except for client pools.

This function can only be called once on a pool, and must be called before the first :symbol:`mongoc_client_pool_pop <mongoc_client_pool_pop>`.

Also note that :symbol:`mongoc_client_set_appname() <mongoc_client_set_appname>` cannot be called on a client retrieved from a client pool.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t <mongoc_client_pool_t>`.
* ``appname``: The application name, of length at most ``MONGOC_HANDSHAKE_APPNAME_MAX``.

Returns
-------

true if the appname is set successfully. Otherwise, false.

