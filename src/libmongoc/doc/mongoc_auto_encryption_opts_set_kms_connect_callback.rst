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
    const mongoc_kms_connect_callback_t *callback);

Set a callback that is invoked whenever the auto-encrypted client needs to
open a network connection to a KMS server.  See
:doc:`mongoc_client_encryption_opts_set_kms_connect_callback` for a full
description of the callback contract and proxy-tunnelling use case.

Parameters
----------

- ``opts`` - The options object to update.
- ``callback`` - The connect callback to set.  A copy is stored in ``opts``, so
  ``callback`` may be destroyed immediately after this call.  May be ``NULL`` to
  clear a previously set callback.  Refer to:
  :doc:`mongoc_kms_connect_callback_t`.

.. seealso::

  - :doc:`mongoc_client_encryption_opts_set_kms_connect_callback`
  - :doc:`mongoc_kms_connect_callback_t`
  - :doc:`mongoc_kms_connect_callback_fn_t`
