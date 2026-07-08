:man_page: mongoc_client_encryption_encrypt_opts_set_algorithm

mongoc_client_encryption_encrypt_opts_set_algorithm()
=====================================================

Synopsis
--------

.. code-block:: c

   #define MONGOC_AEAD_AES_256_CBC_HMAC_SHA_512_RANDOM "AEAD_AES_256_CBC_HMAC_SHA_512-Random"
   #define MONGOC_AEAD_AES_256_CBC_HMAC_SHA_512_DETERMINISTIC "AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic"
   #define MONGOC_ENCRYPT_ALGORITHM_INDEXED "Indexed"
   #define MONGOC_ENCRYPT_ALGORITHM_UNINDEXED "Unindexed"
   #define MONGOC_ENCRYPT_ALGORITHM_RANGE "Range"
   #define MONGOC_ENCRYPT_ALGORITHM_RANGEPREVIEW "RangePreview" // Deprecated.
   #define MONGOC_ENCRYPT_ALGORITHM_TEXTPREVIEW "TextPreview" // Deprecated.
   #define MONGOC_ENCRYPT_ALGORITHM_STRING "String"

   void
   mongoc_client_encryption_encrypt_opts_set_algorithm (
      mongoc_client_encryption_encrypt_opts_t *opts, const char *algorithm);

Sets the algorithm for explicit encryption.
   
Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_opts_t`
* ``algorithm``: A ``char *`` identifying the algorithm.

.. seealso::

  | See `CSFLE explicit encryption <explicit-encryption-csfle>`_ and `Queryable Encryption <explicit-encryption-qe>`_ for algorithm usage.
