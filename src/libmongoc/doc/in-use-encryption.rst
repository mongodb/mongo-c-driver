In-Use Encryption
=================

In-Use Encryption consists of two features:

.. toctree::
   :titlesonly:
   :maxdepth: 1

   client-side-field-level-encryption
   queryable-encryption

Installation
------------

Using In-Use Encryption in the C driver requires the dependency libmongocrypt. See the MongoDB Manual for `libmongocrypt installation instructions <https://www.mongodb.com/docs/manual/core/csfle/reference/libmongocrypt/>`_.

Once libmongocrypt is installed, configure the C driver with ``-DENABLE_CLIENT_SIDE_ENCRYPTION=ON`` to require In-Use Encryption be enabled.

.. parsed-literal::

  $ cd mongo-c-driver
  $ mkdir cmake-build && cd cmake-build
  $ cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF -DENABLE_CLIENT_SIDE_ENCRYPTION=ON ..
  $ cmake --build . --target install

API
---

:symbol:`mongoc_client_encryption_t` is used for explicit encryption and key management.
:symbol:`mongoc_client_enable_auto_encryption()` and :symbol:`mongoc_client_pool_enable_auto_encryption()` is used to enable automatic encryption.

The Queryable Encryption and CSFLE features share much of the same API with some exceptions.

- The supported algorithms documented in :symbol:`mongoc_client_encryption_encrypt_opts_set_algorithm` do not apply to both features.
- :symbol:`mongoc_auto_encryption_opts_set_encrypted_fields_map` only applies to Queryable Encryption.
- :symbol:`mongoc_auto_encryption_opts_set_schema_map` only applies to CSFLE.



.. _query_analysis:

Query Analysis
--------------

To support the automatic encryption feature, one of the following dependencies are required:

- The ``mongocryptd`` executable. See the MongoDB Manual documentation: `Install and Configure mongocryptd <https://www.mongodb.com/docs/manual/core/queryable-encryption/reference/mongocryptd/>`_
- The ``crypt_shared`` library. See the MongoDB Manual documentation: `Automatic Encryption Shared Library <https://www.mongodb.com/docs/manual/core/queryable-encryption/reference/shared-library/>`_

A :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` configured with auto encryption will automatically try to load the ``crypt_shared`` library. If loading the ``crypt_shared`` library fails, the :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` will try to spawn the ``mongocryptd`` process from the application's ``PATH``. To configure use of ``crypt_shared`` and ``mongocryptd`` see :symbol:`mongoc_auto_encryption_opts_set_extra()`.
