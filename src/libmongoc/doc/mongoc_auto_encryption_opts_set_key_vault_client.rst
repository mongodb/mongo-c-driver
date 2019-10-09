:man_page: mongoc_auto_encryption_opts_set_key_vault_client

mongoc_auto_encryption_opts_set_key_vault_client()
==================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_auto_encryption_opts_set_key_vault_client (
      mongoc_auto_encryption_opts_t *opts, struct _mongoc_client_t *client);

Set an optional separate :symbol:`mongoc_client_t` to use during key lookup for automatic encryption and decryption.

Parameters
----------

* ``opts``: A :symbol:`mongoc_auto_encryption_opts_t`.
* ``client``: A :symbol:`mongoc_client_t` to use for key queries. This client should *not* have automatic encryption enabled, as it will only execute ``find`` commands against the key vault collection to retrieve keys for automatic encryption and decryption. This ``client`` MUST outlive any :symbol:`mongoc_client_t` which has been enabled to use it through :symbol:`mongoc_client_enable_auto_encryption()`.

See also
--------

* :symbol:`mongoc_client_enable_auto_encryption()`
* The guide for :doc:`Using Client-Side Field Level Encryption <using_client_side_encryption>`
