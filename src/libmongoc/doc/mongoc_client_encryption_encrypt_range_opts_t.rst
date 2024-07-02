:man_page: mongoc_client_encryption_encrypt_range_opts_t

mongoc_client_encryption_encrypt_range_opts_t
=============================================

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_client_encryption_encrypt_range_opts_t mongoc_client_encryption_encrypt_range_opts_t;

.. versionadded:: 1.24.0

RangeOpts specifies index options for a Queryable Encryption field supporting "range" queries. Used to set options for :symbol:`mongoc_client_encryption_encrypt()`.

The options min, max, trim factor, sparsity, and range must match the values set in the encryptedFields of the destination collection.

For double and decimal128 fields, min/max/precision must all be set, or all be unset.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_client_encryption_encrypt_range_opts_new
    mongoc_client_encryption_encrypt_range_opts_destroy
    mongoc_client_encryption_encrypt_range_opts_set_trim_factor
    mongoc_client_encryption_encrypt_range_opts_set_sparsity
    mongoc_client_encryption_encrypt_range_opts_set_min
    mongoc_client_encryption_encrypt_range_opts_set_max
    mongoc_client_encryption_encrypt_range_opts_set_precision
    mongoc_client_encryption_encrypt_opts_set_range_opts

.. seealso::

  | :symbol:`mongoc_client_encryption_encrypt()`
  | :symbol:`mongoc_client_encryption_encrypt_opts_t()`
