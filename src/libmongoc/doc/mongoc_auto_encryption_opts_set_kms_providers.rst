:man_page: mongoc_auto_encryption_opts_set_kms_providers

mongoc_auto_encryption_opts_set_kms_providers()
===============================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_auto_encryption_opts_set_kms_providers (
      mongoc_auto_encryption_opts_t *opts, const bson_t *kms_providers);


Parameters
----------

* ``opts``: The :symbol:`mongoc_auto_encryption_opts_t`
* ``kms_providers``: A :symbol:`bson_t` containing configuration for an external Key Management Service (KMS).

``kms_providers`` is a BSON document containing configuration for each KMS provider. Currently ``aws`` or ``local`` are supported. At least one must be specified.

The format for "aws" is as follows:

.. code-block:: javascript

   aws: {
      accessKeyId: <string>,
      secretAccessKey: <string>
   }

The format for "local" is as follows:

.. code-block:: javascript

   local: {
      key: <96 byte BSON binary of subtype 0> // The master key used to encrypt/decrypt data keys.
   }

See also
--------

* :symbol:`mongoc_client_enable_auto_encryption()`
* The guide for :doc:`Using Client-Side Field Level Encryption <using_client_side_encryption>`
