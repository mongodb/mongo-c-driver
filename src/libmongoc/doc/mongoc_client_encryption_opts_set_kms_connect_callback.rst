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
  set callback.  Refer to: :doc:`mongoc_kms_connect_callback_fn`.
- ``userdata`` - An arbitrary pointer passed unchanged to ``fn`` each time it
  is called.

.. seealso::

  - :doc:`mongoc_auto_encryption_opts_set_kms_connect_callback`
  - :doc:`mongoc_kms_connect_callback_fn`
