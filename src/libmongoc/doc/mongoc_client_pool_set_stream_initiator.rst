:man_page: mongoc_client_pool_set_stream_initiator

mongoc_client_pool_set_stream_initiator()
=========================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_client_pool_set_stream_initiator (mongoc_client_pool_t *pool,
                                         mongoc_stream_initiator_t initiator,
                                         void *user_data);

Sets a custom stream initiator for clients retrieved from ``pool`` and for
background server monitoring. This completely replaces the default transport,
including TLS. The initiator is responsible for returning streams that fulfill
the :symbol:`mongoc_stream_t` contract and applying any required TLS configuration.

Call this function before retrieving the first client with
:symbol:`mongoc_client_pool_pop()` or :symbol:`mongoc_client_pool_try_pop()`.
The initiator may be replaced before the first client is created. Once a client
has been created, the initiator cannot be changed, even if all clients have
been returned to the pool.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.
* ``initiator``: A non-NULL :symbol:`mongoc_stream_initiator_t <mongoc_client_t>`.
* ``user_data``: Passed to each invocation of ``initiator``. May be NULL.

Returns
-------

Returns true if the initiator was set. If a client has already been created,
returns false and logs an error without changing the initiator or its user data.

Thread Safety
-------------

This function is safe to call from multiple threads. Configure the initiator
before other threads begin retrieving clients.

The initiator may be called concurrently by application threads and background
monitoring threads. The application must synchronize access to shared user data
and keep it valid until :symbol:`mongoc_client_pool_destroy()` returns.
