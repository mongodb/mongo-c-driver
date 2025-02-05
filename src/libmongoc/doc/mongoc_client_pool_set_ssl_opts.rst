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

.. note::
   |ssl:naming|

This function is identical to :symbol:`mongoc_client_set_ssl_opts()` except for
client pools. It ensures that all clients retrieved from
:symbol:`mongoc_client_pool_pop()` or :symbol:`mongoc_client_pool_try_pop()`
are configured with the same TLS settings.

The :symbol:`mongoc_ssl_opt_t` struct is copied by the pool along with the strings
it points to (``pem_file``, ``pem_pwd``, ``ca_file``, ``ca_dir``, and
``crl_file``) so they don't have to remain valid after the call to
:symbol:`mongoc_client_pool_set_ssl_opts`.

A call to :symbol:`mongoc_client_pool_set_ssl_opts` overrides all TLS options set
through the connection string with which the :symbol:`mongoc_client_pool_t` was
constructed.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.
* ``opts``: A :symbol:`mongoc_ssl_opt_t`.

.. include:: includes/mongoc_client_pool_call_once.txt

Availability
------------

This feature requires that the MongoDB C driver was compiled with ``-DENABLE_SSL``.

