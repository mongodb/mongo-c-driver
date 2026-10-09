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

Decreasing the maximum immediately destroys idle clients until the total number
of clients is within the new limit or no idle clients remain. Checked-out
clients are not interrupted. If they keep the pool above the new limit, they
are destroyed as they are returned with :symbol:`mongoc_client_pool_push()`
until the pool is within the limit.

Setting the maximum to zero destroys all idle clients and destroys checked-out
clients as they are returned. :symbol:`mongoc_client_pool_try_pop()` then
returns ``NULL`` and :symbol:`mongoc_client_pool_pop()` waits for the maximum
to increase, subject to ``waitQueueTimeoutMS``. The pool can be used again
after its maximum is increased. This describes the setter; it does not change
the interpretation of ``maxPoolSize`` in a connection string.

A pool drained to zero can also be destroyed without increasing its maximum.
In that case, no client is available for the best-effort ``endSessions`` command,
so it is not sent.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.
* ``max_pool_size``: The maximum number of connections which shall be available from the pool.

.. include:: includes/mongoc_client_pool_thread_safe.txt
