:man_page: mongoc_kms_connect_callback_fn_t

mongoc_kms_connect_callback_fn_t
================================

Synopsis
--------

.. code-block:: c

  typedef mongoc_stream_t *(*mongoc_kms_connect_callback_fn_t) (
     mongoc_kms_connect_callback_params_t *params);

The type of the function pointer stored by :symbol:`mongoc_kms_connect_callback_t`. It opens a transport
connection to a KMS endpoint.

The driver calls this function instead of opening a direct TCP connection to the
host and port described by ``params``.  After the callback returns a connected
stream, the driver wraps it with TLS before sending any KMS request.

The primary use case is routing KMS traffic through an HTTP ``CONNECT`` proxy:
the callback opens a connection to the proxy, performs the ``CONNECT``
handshake to establish a tunnel to the KMS endpoint, then returns the tunnel
socket.

The callback chooses its own deadline for establishing the connection.

Parameters
----------

- ``params`` - A :symbol:`mongoc_kms_connect_callback_params_t`.  Use its
  accessors to obtain the KMS host and port to connect to and the user data
  stored by the :symbol:`mongoc_kms_connect_callback_t`. Only valid for the
  duration of the call.

Returns
-------

A connected :symbol:`mongoc_stream_t` on success, or ``NULL`` on failure. When
returning ``NULL``, call :symbol:`mongoc_kms_connect_callback_params_set_error`
and return the ``NULL`` it returns.

Example
-------

The following implementation of :symbol:`mongoc_kms_connect_callback_fn_t` opens a connection to an HTTP
``CONNECT`` proxy, then performs the ``CONNECT`` handshake to establish a tunnel to the KMS endpoint:

.. literalinclude:: ../examples/client-side-encryption-kms-http-proxy.c
   :caption: Excerpt from examples/client-side-encryption-kms-http-proxy.c
   :start-after: BEGIN:mongoc_kms_connect_callback_fn_t
   :end-before: END:mongoc_kms_connect_callback_fn_t

See the full example, which registers this callback on a :symbol:`mongoc_client_encryption_opts_t`, in
``examples/client-side-encryption-kms-http-proxy.c``.

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_t`
  - :symbol:`mongoc_kms_connect_callback_params_t`
  - :symbol:`mongoc_client_encryption_opts_set_kms_connect_callback`
  - :symbol:`mongoc_auto_encryption_opts_set_kms_connect_callback`
