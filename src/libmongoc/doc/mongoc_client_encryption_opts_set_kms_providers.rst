:man_page: mongoc_client_encryption_opts_set_kms_providers

mongoc_client_encryption_opts_set_kms_providers()
=================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_opts_set_kms_providers (
      mongoc_client_encryption_opts_t *opts, const bson_t *kms_providers);

Parameters
----------

* ``opts``: The :symbol:`mongoc_client_encryption_opts_t`
* ``kms_providers``: A :symbol:`bson_t` containing configuration for an external Key Management Service (KMS).

``kms_providers`` is a BSON document containing configuration for each KMS provider.

.. include:: includes/kms_providers.txt

.. seealso::

  | :symbol:`mongoc_client_encryption_new()`

  | `In-Use Encryption <in-use-encryption_>`_

