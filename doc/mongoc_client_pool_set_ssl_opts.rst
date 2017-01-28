:man_page: mongoc_client_pool_set_ssl_opts

mongoc_client_pool_set_ssl_opts()
=================================

Synopsis
--------

.. code-block:: c

  #ifdef MONGOC_ENABLE_SSL
  void
  mongoc_client_pool_set_ssl_opts (mongoc_client_pool_t *pool,
                                   const mongoc_ssl_opt_t *opts);
  #endif

This function is identical to :symbol:`mongoc_client_set_ssl_opts()` except for client pools. It ensures that all clients retrieved from :symbol:`mongoc_client_pool_pop()` or :symbol:`mongoc_client_pool_try_pop()` are configured with the same SSL settings.

Beginning in version 1.2.0, once a pool has any SSL options set, all connections use SSL, even if ``ssl=true`` is omitted from the MongoDB URI. Before, SSL options were ignored unless ``ssl=true`` was included in the URI.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.
* ``opts``: A :symbol:`mongoc_ssl_opt_t` that will not be modified.

.. include:: includes/mongoc_client_pool_call_once.txt

Availability
------------

This feature requires that the MongoDB C driver was compiled with ``--enable-ssl``.

