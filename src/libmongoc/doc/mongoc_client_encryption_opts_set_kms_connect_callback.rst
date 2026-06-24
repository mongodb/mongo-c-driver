:man_page: mongoc_client_encryption_opts_set_kms_connect_callback

mongoc_client_encryption_opts_set_kms_connect_callback ()
=========================================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  void
  mongoc_client_encryption_opts_set_kms_connect_callback (
    mongoc_client_encryption_opts_t *opts,
    mongoc_kms_connect_callback_fn fn,
    void *userdata);

Set a callback that is invoked whenever the driver needs to open a network
connection to a KMS server.  The callback is responsible for establishing the
transport-layer connection (plain TCP or one tunnelled through an HTTP proxy)
and returning the raw socket as a :symbol:`mongoc_stream_t`.  After the
callback returns, the driver wraps that stream with TLS before sending any KMS
requests.

This is the primary extension point for routing KMS traffic through an HTTP
``CONNECT`` proxy.

Parameters
----------

- ``opts`` - The options object to update.
- ``fn`` - The connect callback to set.  May be ``NULL`` to clear a previously
  set callback.  Refer to: :c:type:`mongoc_kms_connect_callback_fn`.
- ``userdata`` - An arbitrary pointer passed unchanged to ``fn`` each time it
  is called.

.. seealso:: :doc:`mongoc_auto_encryption_opts_set_kms_connect_callback`

.. rubric:: Related:

.. c:type:: mongoc_kms_connect_callback_fn

  .. code-block:: c

    typedef
    mongoc_stream_t *(*mongoc_kms_connect_callback_fn) (const char *host,
                                                        int32_t port,
                                                        void *userdata,
                                                        bson_error_t *error);

  The type of a callback that opens a transport connection to a KMS endpoint.

  The driver calls this function instead of opening a direct TCP connection to
  ``host:port``.  The callback should:

  1. Open a connection (plain TCP or via a proxy ``CONNECT`` tunnel) to the
     target ``host`` and ``port``.
  2. Return the connected :symbol:`mongoc_stream_t`.

  The driver will wrap the returned stream with TLS before sending KMS
  requests.

  :parameters:

    - ``host`` - The KMS hostname the driver needs to reach (e.g.
      ``"kms.us-east-1.amazonaws.com"``).
    - ``port`` - The KMS port number (typically ``443``).
    - ``userdata`` - The same pointer provided to the ``userdata`` parameter
      when the callback was registered.
    - ``error`` - Output parameter.  On failure, set a descriptive error here.

  :return value: A connected :symbol:`mongoc_stream_t` on success, or
    ``NULL`` on failure (``error`` must be set).
