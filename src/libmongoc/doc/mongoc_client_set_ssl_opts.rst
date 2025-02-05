:man_page: mongoc_client_set_ssl_opts

mongoc_client_set_ssl_opts()
============================

Synopsis
--------

.. code-block:: c

  #ifdef MONGOC_ENABLE_SSL
  void
  mongoc_client_set_ssl_opts (mongoc_client_t *client,
                              const mongoc_ssl_opt_t *opts);
  #endif

.. note::
   |ssl:naming|

Sets the TLS (SSL) options to use when connecting to TLS enabled MongoDB servers.

The :symbol:`mongoc_ssl_opt_t` struct is copied by the client along with the strings
it points to (``pem_file``, ``pem_pwd``, ``ca_file``, ``ca_dir``, and
``crl_file``) so they don't have to remain valid after the call to
:symbol:`mongoc_client_set_ssl_opts`.

A call to :symbol:`mongoc_client_set_ssl_opts` overrides all TLS options set through
the connection string with which the :symbol:`mongoc_client_t` was constructed.

It is a programming error to call this function on a client from a
:symbol:`mongoc_client_pool_t`. Instead, call
:symbol:`mongoc_client_pool_set_ssl_opts` on the pool before popping any
clients.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``opts``: A :symbol:`mongoc_ssl_opt_t`.

Availability
------------

This feature requires that the MongoDB C driver was compiled with ``-DENABLE_SSL``.

