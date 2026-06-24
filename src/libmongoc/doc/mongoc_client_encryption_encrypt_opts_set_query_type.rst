:man_page: mongoc_client_encryption_encrypt_opts_set_query_type

mongoc_client_encryption_encrypt_opts_set_query_type()
======================================================

Synopsis
--------

.. code-block:: c

   #define MONGOC_ENCRYPT_QUERY_TYPE_EQUALITY "equality"
   #define MONGOC_ENCRYPT_QUERY_TYPE_RANGE "range"
   #define MONGOC_ENCRYPT_QUERY_TYPE_RANGEPREVIEW "rangePreview" // Deprecated.
   #define MONGOC_ENCRYPT_QUERY_TYPE_SUBSTRINGPREVIEW "substringPreview"
   #define MONGOC_ENCRYPT_QUERY_TYPE_PREFIXPREVIEW "prefixPreview" // Deprecated.
   #define MONGOC_ENCRYPT_QUERY_TYPE_PREFIX "prefix"
   #define MONGOC_ENCRYPT_QUERY_TYPE_SUFFIXPREVIEW "suffixPreview" // Deprecated.
   #define MONGOC_ENCRYPT_QUERY_TYPE_SUFFIX "suffix"

   void mongoc_client_encryption_encrypt_opts_set_query_type (
      mongoc_client_encryption_encrypt_opts_t *opts, const char* query_type);

.. versionadded:: 1.22.0

Sets the query type for explicit encryption.

.. warning::
   The "substringPreview" query type is in preview and should be used for experimental workloads only. This feature is unstable and its security is not guaranteed until released as Generally Available (GA). The GA version of this feature may not be backwards compatible with the preview version.

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_opts_t`
* ``query_type``: A query type to use for explicit encryption.

.. seealso::

  | See `CSFLE explicit encryption <explicit-encryption-csfle>`_ and `Queryable Encryption <explicit-encryption-qe>`_ for query type usage.
