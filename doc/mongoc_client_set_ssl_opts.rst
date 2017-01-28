:man_page: mongoc_client_set_ssl_opts

mongoc_client_set_ssl_opts()
============================

Synopsis
--------

.. code-block:: c

  #ifdef MONGOC_ENABLE_SSL
  mongoc_client_set_ssl_opts (mongoc_client_t *client,
                              const mongoc_ssl_opt_t *opts);
  #endif

Sets the SSL options to use when connecting to SSL enabled MongoDB servers.

Beginning in version 1.2.0, once a client has any SSL options set, all connections use SSL, even if ``ssl=true`` is omitted from the MongoDB URI. Before, SSL options were ignored unless ``ssl=true`` was included in the URI.

The ``mongoc_ssl_opt_t`` struct is copied by the client along with the strings it points to (``pem_file``, ``pem_pwd``, ``ca_file``, ``ca_dir``, and ``crl_file``) so they don't have to remain valid after the call to ``mongoc_client_set_ssl_opts``.

It is a programming error to call this function on a client from a :symbol:`mongoc_client_pool_t`. Instead, call :symbol:`mongoc_client_pool_set_ssl_opts` on the pool before popping any clients.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``opts``: A :symbol:`mongoc_ssl_opt_t`. The struct is copied by the client, but the strings it points to are not.

Availability
------------

This feature requires that the MongoDB C driver was compiled with ``--enable-ssl``.

