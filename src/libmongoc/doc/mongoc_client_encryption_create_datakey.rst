:man_page: mongoc_client_encryption_create_datakey

mongoc_client_encryption_create_datakey()
=========================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_client_encryption_create_datakey (
      mongoc_client_encryption_t *client_encryption,
      const char *kms_provider,
      mongoc_client_encryption_datakey_opts_t *opts,
      bson_value_t *keyid,
      bson_error_t *error);

Alias function equivalent to :symbol:`mongoc_client_encryption_create_key`.

.. seealso::

  | :symbol:`mongoc_client_encryption_datakey_opts_t`
