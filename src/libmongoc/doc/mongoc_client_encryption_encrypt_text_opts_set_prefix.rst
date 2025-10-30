:man_page: mongoc_client_encryption_encrypt_text_opts_set_prefix

mongoc_client_encryption_encrypt_text_opts_set_prefix()
=====================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_text_opts_set_prefix(
      mongoc_client_encryption_encrypt_text_opts_t *opts,
      mongoc_client_encryption_encrypt_text_prefix_opts_t *popts);

.. versionadded:: 2.2.0

Sets the prefix options for text search encryption.

|encrypt-text-is-experimental|

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_text_opts_t`.
* ``popts``: A :symbol:`mongoc_client_encryption_encrypt_text_prefix_opts_t` to set as prefix options.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_text_opts_new`
   | :symbol:`mongoc_client_encryption_encrypt_text_prefix_opts_new`
