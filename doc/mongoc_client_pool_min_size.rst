:man_page: mongoc_client_pool_min_size

mongoc_client_pool_min_size()
=============================

Synopsis
--------

.. code-block:: none

  void
  mongoc_client_pool_min_size(mongoc_client_pool_t *pool,
                              uint32_t              min_pool_size);

This function sets the minimum number of pooled connections kept in :symbol:`mongoc_client_pool_t <mongoc_client_pool_t>`.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t <mongoc_client_pool_t>`.
* ``min_pool_size``: The minimum number of connections which shall be kept in the pool.

