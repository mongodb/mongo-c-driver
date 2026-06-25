:man_page: mongoc_kms_connect_callback_fn

mongoc_kms_connect_callback_fn
==============================

Synopsis
--------

.. code-block:: c

  typedef mongoc_stream_t *(*mongoc_kms_connect_callback_fn) (const char *host,
                                                              uint16_t port,
                                                              int32_t connecttimeoutms,
                                                              void *userdata,
                                                              bson_error_t *error);

The type of the callback that opens a transport connection to a KMS endpoint,
used by :symbol:`mongoc_client_encryption_opts_set_kms_connect_callback` and
:symbol:`mongoc_auto_encryption_opts_set_kms_connect_callback`.

The driver calls this function instead of opening a direct TCP connection to
``host:port``.  After the callback returns a connected stream, the driver wraps
it with TLS before sending any KMS request.

The primary use case is routing KMS traffic through an HTTP ``CONNECT`` proxy:
the callback opens a connection to the proxy, performs the ``CONNECT``
handshake to establish a tunnel to ``host:port``, then returns the tunnel
socket.

Parameters
----------

- ``host`` - The KMS hostname the driver needs to reach (e.g.
  ``"kms.us-east-1.amazonaws.com"``).
- ``port`` - The KMS port number (typically ``443``).
- ``connecttimeoutms`` - The connect timeout in milliseconds.  Use this as the
  deadline for establishing the transport connection (e.g. pass it to
  :symbol:`mongoc_client_connect_tcp` and to
  :symbol:`mongoc_stream_tls_handshake_block`).
- ``userdata`` - The pointer supplied to ``userdata`` when the callback was
  registered.
- ``error`` - Output parameter.  Set a descriptive error message and domain/code
  here when returning ``NULL``.

Returns
-------

A connected :symbol:`mongoc_stream_t` on success, or ``NULL`` on failure
(``error`` must be set).

.. seealso::

  - :symbol:`mongoc_client_encryption_opts_set_kms_connect_callback`
  - :symbol:`mongoc_auto_encryption_opts_set_kms_connect_callback`
