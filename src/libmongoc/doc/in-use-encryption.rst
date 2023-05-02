In-Use Encryption
=================

In-Use Encryption consists of two features:

.. toctree::
   :titlesonly:
   :maxdepth: 1

   using_client_side_encryption
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

Query Analysis
--------------

To support the automatic encryption feature, one of the following dependencies are required:

- The ``mongocryptd`` executable. See the MongoDB Manual documentation: `Install and Configure mongocryptd <https://www.mongodb.com/docs/manual/core/queryable-encryption/reference/mongocryptd/>`_
- The ``crypt_shared`` library. See the MongoDB Manual documentation: `Automatic Encryption Shared Library <https://www.mongodb.com/docs/manual/core/queryable-encryption/reference/shared-library/>`_

A :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` configured with auto encryption will automatically try to load the ``crypt_shared`` library. If loading the ``crypt_shared`` library fails, the :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` will try to spawn the ``mongocryptd`` process from the application's ``PATH``. To configure use of ``crypt_shared`` and ``mongocryptd`` see :symbol:`mongoc_auto_encryption_opts_set_extra()`.
