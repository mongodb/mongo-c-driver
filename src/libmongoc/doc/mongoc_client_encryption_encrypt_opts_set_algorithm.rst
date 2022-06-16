:man_page: mongoc_client_encryption_encrypt_opts_set_algorithm

mongoc_client_encryption_encrypt_opts_set_algorithm()
=====================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_opts_set_algorithm (
      mongoc_client_encryption_encrypt_opts_t *opts, const char *algorithm);

   #define MONGOC_AEAD_AES_256_CBC_HMAC_SHA_512_RANDOM "AEAD_AES_256_CBC_HMAC_SHA_512-Random"
   #define MONGOC_AEAD_AES_256_CBC_HMAC_SHA_512_DETERMINISTIC "AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic"
   // (Experimental: See below)
   #define MONGOC_ENCRYPT_ALGORITHM_INDEXED "Indexed"
   // (Experimental: See below)
   #define MONGOC_ENCRYPT_ALGORITHM_UNINDEXED "Unindexed"

Identifies the algorithm to use for encryption. Valid values of ``algorithm`` are:

``"AEAD_AES_256_CBC_HMAC_SHA_512-Random"``

   for randomized encryption.

``"AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic"``

   for deterministic (queryable) encryption.

``"Indexed"``

   for indexed encryption.

   .. note:: |qenc:opt-is-experimental|

``"Unindexed"``

   for unindexed encryption.

   .. note:: |qenc:opt-is-experimental|

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_opts_t`
* ``algorithm``: A ``char *`` identifying the algorithm.