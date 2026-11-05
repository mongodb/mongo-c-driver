:man_page: mongoc_client_pool_get_stats

mongoc_client_pool_get_stats()
==============================

Synopsis
--------

.. code-block:: c

  void
  mongoc_client_pool_get_stats (mongoc_client_pool_t *pool,
                                mongoc_client_pool_stats_t *stats);

Get cumulative and current client counts for a pool.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.
* ``stats``: An output :symbol:`mongoc_client_pool_stats_t`.

The output is a consistent snapshot read while holding the pool mutex. See
:symbol:`mongoc_client_pool_stats_t` for field descriptions. The counts describe
client objects and do not represent network connections.
