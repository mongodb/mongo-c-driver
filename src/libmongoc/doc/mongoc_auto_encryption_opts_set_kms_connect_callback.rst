:man_page: mongoc_auto_encryption_opts_set_kms_connect_callback

mongoc_auto_encryption_opts_set_kms_connect_callback()
======================================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  void
  mongoc_auto_encryption_opts_set_kms_connect_callback (
    mongoc_auto_encryption_opts_t *opts,
    mongoc_kms_connect_callback_fn fn,
    void *userdata);

Set a callback that is invoked whenever the auto-encrypted client needs to
open a network connection to a KMS server.  See
:doc:`mongoc_client_encryption_opts_set_kms_connect_callback` for a full
description of the callback contract and proxy-tunnelling use case.

Parameters
----------

- ``opts`` - The options object to update.
- ``fn`` - The connect callback to set.  May be ``NULL`` to clear a previously
  set callback.  Refer to:
  :doc:`mongoc_kms_connect_callback_fn`
- ``userdata`` - An arbitrary pointer passed unchanged to ``fn`` each time it
  is called.

.. seealso::

  - :doc:`mongoc_client_encryption_opts_set_kms_connect_callback`
  - :doc:`mongoc_kms_connect_callback_fn`
