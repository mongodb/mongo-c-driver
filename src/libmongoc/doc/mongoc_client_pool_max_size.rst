:man_page: mongoc_client_pool_max_size

mongoc_client_pool_max_size()
=============================

Synopsis
--------

.. code-block:: c

  void
  mongoc_client_pool_max_size (mongoc_client_pool_t *pool,
                               uint32_t max_pool_size);

This function sets the maximum number of pooled connections available from a :symbol:`mongoc_client_pool_t`.

Increasing the maximum wakes threads waiting in :symbol:`mongoc_client_pool_pop()`
so they can create clients up to the new limit without waiting for a client to
be returned to the pool.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.
* ``max_pool_size``: The maximum number of connections which shall be available from the pool.

.. include:: includes/mongoc_client_pool_thread_safe.txt
