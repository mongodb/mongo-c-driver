:man_page: mongoc_client_pool_warmup

mongoc_client_pool_warmup()
===========================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_client_pool_warmup (mongoc_client_pool_t *pool, size_t num_to_warmup);

Eagerly connect ``num_to_warmup`` clients in the pool to the MongoDB servers in the pool's topology.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.
* ``num_to_warmup``: The number of clients to eagerly connect to the MongoDB servers in the pool's topology.

.. include:: includes/mongoc_client_pool_thread_safe.txt
