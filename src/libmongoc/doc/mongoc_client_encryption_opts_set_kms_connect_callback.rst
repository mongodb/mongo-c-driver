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
    const mongoc_kms_connect_callback_t *callback);

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
- ``callback`` - The connect callback to set.  A copy is stored in ``opts``, so
  ``callback`` may be destroyed immediately after this call.  May be ``NULL`` to
  clear a previously set callback.  Refer to:
  :doc:`mongoc_kms_connect_callback_t`.

Example: Routing KMS Traffic Through an HTTP CONNECT Proxy
------------------------------------------------------------

Registers a :symbol:`mongoc_kms_connect_callback_t` on a :symbol:`mongoc_client_encryption_opts_t` so that all KMS
requests made by the resulting :symbol:`mongoc_client_encryption_t` are tunnelled through an HTTP ``CONNECT``
proxy:

.. literalinclude:: ../examples/client-side-encryption-kms-http-proxy.c
   :caption: Excerpt from examples/client-side-encryption-kms-http-proxy.c
   :start-after: BEGIN:mongoc_client_encryption_opts_set_kms_connect_callback
   :end-before: END:mongoc_client_encryption_opts_set_kms_connect_callback
   :dedent: 3

See the full example, including the implementation of ``kms_connect_via_http_proxy`` that performs the
``CONNECT`` handshake, in ``examples/client-side-encryption-kms-http-proxy.c``.

.. seealso::

  - :doc:`mongoc_auto_encryption_opts_set_kms_connect_callback`
  - :doc:`mongoc_kms_connect_callback_t`
  - :doc:`mongoc_kms_connect_callback_fn_t`
