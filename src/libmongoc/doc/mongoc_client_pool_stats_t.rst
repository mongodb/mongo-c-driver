:man_page: mongoc_client_pool_stats_t

mongoc_client_pool_stats_t
==========================

Synopsis
--------

.. code-block:: c

  typedef struct {
     uint64_t clients_created;
     uint64_t clients_destroyed;
     uint64_t clients_in_pool;
  } mongoc_client_pool_stats_t;

Statistics returned by :symbol:`mongoc_client_pool_get_stats`.

Fields
------

* ``clients_created``: The cumulative number of client objects created by the pool.
* ``clients_destroyed``: The cumulative number of client objects destroyed by the pool.
* ``clients_in_pool``: The current number of client objects owned by the pool, including checked-out clients.

These statistics describe :symbol:`mongoc_client_t` objects, not network connections.
The counters are updated when the pool creates or destroys a client. Statistics are
read together under the pool mutex. The application must return all checked-out
clients before destroying the pool.
