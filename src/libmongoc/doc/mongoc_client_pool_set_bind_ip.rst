:man_page: mongoc_client_pool_set_bind_ip

mongoc_client_pool_set_bind_ip()
================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_client_pool_set_bind_ip (mongoc_client_pool_t *pool, const char *bind_ip, bson_error_t *error);

Sets a custom bind IP for ``pool``.  All clients from ``pool`` will bind() all outgoing TCP socket connections to the address ``bind_ip`` before calling connect(). Call this method immediately after creating ``pool`` and before getting a :symbol:`mongoc_client_t` from ``pool`` to ensure that no connections are opened before the custom IP address is set.

``bind_ip`` is copied, and does not have to remain valid after the call to ``mongoc_client_pool_set_bind_ip()``.

This function will set ``error`` and return false in the following cases:

* ``bind_ip`` is not a valid IPv4 address

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.
* ``bind_ip``: A custom IP address.
* ``error``: A :symbol:`bson_error_t`.

Returns
-------

true if the address is set successfully. Otherwise, false.
