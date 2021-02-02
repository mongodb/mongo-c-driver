:man_page: mongoc_client_set_bind_ip

mongoc_client_set_bind_ip()
===========================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_client_set_bind_ip (mongoc_client_t *client, const char *bind_ip, bson_error_t *error);

Sets a custom bind IP for ``client``.  ``client`` will bind() all outgoing TCP socket connections to the address ``bind_ip`` before calling connect(). Call this method immediately after creating ``client`` and before using ``client`` to perform any operations to ensure that it does not open connections before the custom IP address is set.

``bind_ip`` is copied, and does not have to remain valid after the call to ``mongoc_client_set_bind_ip()``.

Only applies to a single-threaded :symbol:`mongoc_client_t`. To use with client pools, see :symbol:`mongoc_client_pool_set_bind_ip()`.

This function will set ``error`` and return false in the following cases:

* ``client`` is from a :symbol:`mongoc_client_pool_t`
* ``bind_ip`` is not a valid IPv4 address

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``bind_ip``: A custom IP address.
* ``error``: A :symbol:`bson_error_t`.

Returns
-------

true if the address is set successfully. Otherwise, false.
