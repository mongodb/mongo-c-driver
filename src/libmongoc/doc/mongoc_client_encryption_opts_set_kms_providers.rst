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

``kms_providers`` is a BSON document containing configuration for each KMS provider. Currently ``aws``, ``local``, ``azure``, ``gcp``, and ``kmip`` are supported. At least one must be specified.

The format for "aws" is as follows:

.. code-block:: javascript

   aws: {
      accessKeyId: String,
      secretAccessKey: String,
      tls : Document
   }

The format for "local" is as follows:

.. code-block:: javascript

   local: {
      key: byte[96] or String // The master key used to encrypt/decrypt data keys. May be passed as a base64 encoded string.
   }

The format for "azure" is as follows:

.. code-block:: javascript

   azure: {
      tenantId: String,
      clientId: String,
      clientSecret: String,
      identityPlatformEndpoint: Optional<String>, // Defaults to login.microsoftonline.com
      tls : Document
   }

The format for "gcp" is as follows:

.. code-block:: javascript

   gcp: {
      email: String,
      privateKey: byte[] or String, // May be passed as a base64 encoded string.
      endpoint: Optional<String>, // Defaults to oauth2.googleapis.com
      tls : Document
   }

The format for "kmip" is as follows:

.. code-block:: javascript

   kmip: {
      endpoint: String,
      tls : Document
   }

The keys of the "tls" document may be the following TLS options:

- MONGOC_URI_TLSCERTIFICATEKEYFILE
- MONGOC_URI_TLSCERTIFICATEKEYFILEPASSWORD
- MONGOC_URI_TLSCAFILE
- MONGOC_URI_TLSALLOWINVALIDCERTIFICATES
- MONGOC_URI_TLSALLOWINVALIDHOSTNAMES
- MONGOC_URI_TLSINSECURE
- MONGOC_URI_TLSDISABLECERTIFICATEREVOCATIONCHECK
- MONGOC_URI_TLSDISABLEOCSPENDPOINTCHECK

See :doc:`configuring_tls` for a description of these options.

.. seealso::

  | :symbol:`mongoc_client_encryption_new()`

  | The guide for :doc:`Using Client-Side Field Level Encryption <using_client_side_encryption>`

