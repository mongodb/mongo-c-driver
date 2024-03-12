:man_page: mongoc_client_reset_sockettimeoutms

mongoc_client_reset_sockettimeoutms()
=======================

Synopsis
--------

.. code-block:: c

  void
  mongoc_client_reset_sockettimeoutms (mongoc_client_t *client);

Reset the sockettimeoutms of the mongoc_cluster_t object associated with the given mongoc_client_t object back to the default value.
Primarily used inside mongoc_client_pool_push() when returning a client to a connection pool.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.

