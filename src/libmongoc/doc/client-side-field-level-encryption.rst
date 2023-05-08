Client-Side Field Level Encryption
==================================

New in MongoDB 4.2, Client-Side Field Level Encryption (also referred to as CSFLE) allows administrators and developers to encrypt specific data fields in addition to other MongoDB encryption features.

With CSFLE, developers can encrypt fields client side without any server-side configuration or directives. CSFLE supports workloads where applications must guarantee that unauthorized parties, including server administrators, cannot read the encrypted data.

Automatic encryption, where sensitive fields in commands are encrypted automatically, requires an Enterprise-only dependency for Query Analysis. See :doc:`In-Use Encryption </in-use-encryption>` for more information.

.. seealso::

    | The MongoDB Manual for `Client-Side Field Level Encryption <https://www.mongodb.com/docs/manual/core/security-client-side-encryption/>`_


Automatic Client-Side Field Level Encryption
--------------------------------------------

Automatic encryption is enabled by calling :symbol:`mongoc_client_enable_auto_encryption()` on a :symbol:`mongoc_client_t`. The following examples show how to set up automatic encryption using :symbol:`mongoc_client_encryption_t` to create a new encryption data key.

.. note::

   Automatic encryption requires MongoDB 4.2 enterprise or a MongoDB 4.2 Atlas cluster. The community version of the server supports automatic decryption as well as :ref:`explicit-client-side-encryption`.

Providing Local Automatic Encryption Rules
``````````````````````````````````````````

The following example shows how to specify automatic encryption rules using a schema map set with :symbol:`mongoc_auto_encryption_opts_set_schema_map()`. The automatic encryption rules are expressed using a strict subset of the JSON Schema syntax.

Supplying a schema map provides more security than relying on JSON Schemas obtained from the server. It protects against a malicious server advertising a false JSON Schema, which could trick the client into sending unencrypted data that should be encrypted.

JSON Schemas supplied in the schema map only apply to configuring automatic encryption. Other validation rules in the JSON schema will not be enforced by the driver and will result in an error:

.. literalinclude:: ../examples/client-side-encryption-schema-map.c
   :caption: client-side-encryption-schema-map.c
   :language: c

Server-Side Field Level Encryption Enforcement
``````````````````````````````````````````````

The MongoDB 4.2 server supports using schema validation to enforce encryption of specific fields in a collection. This schema validation will prevent an application from inserting unencrypted values for any fields marked with the "encrypt" JSON schema keyword.

The following example shows how to set up automatic encryption using :symbol:`mongoc_client_encryption_t` to create a new encryption data key and create a collection with the necessary JSON Schema:

.. literalinclude:: ../examples/client-side-encryption-server-schema.c
   :caption: client-side-encryption-server-schema.c
   :language: c

.. _explicit-client-side-encryption:

Explicit Encryption
```````````````````

Explicit encryption is a MongoDB community feature and does not use :ref:`query_analysis` (``mongocryptd`` or ``crypt_shared``). Explicit encryption is provided by the :symbol:`mongoc_client_encryption_t` class, for example:

.. literalinclude:: ../examples/client-side-encryption-explicit.c
   :caption: client-side-encryption-explicit.c
   :language: c

Explicit Encryption with Automatic Decryption
`````````````````````````````````````````````

Although automatic encryption requires MongoDB 4.2 enterprise or a MongoDB 4.2 Atlas cluster, automatic decryption is supported for all users. To configure automatic decryption without automatic encryption set bypass_auto_encryption=True in :symbol:`mongoc_auto_encryption_opts_t`:

.. literalinclude:: ../examples/client-side-encryption-auto-decryption.c
   :caption: client-side-encryption-auto-decryption.c
   :language: c
