:man_page: mongoc_client_encryption_encrypt_text_opts_set_substring

mongoc_client_encryption_encrypt_text_opts_set_substring()
========================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_text_opts_set_substring(
      mongoc_client_encryption_encrypt_text_opts_t *opts,
      mongoc_client_encryption_encrypt_text_substring_opts_t *ssopts);

.. versionadded:: 2.2.0

Sets the substring options for text search encryption.

|encrypt-text-is-experimental|

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_text_opts_t`.
* ``ssopts``: A :symbol:`mongoc_client_encryption_encrypt_text_substring_opts_t` to set as substring options.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_text_opts_new`
   | :symbol:`mongoc_client_encryption_encrypt_text_substring_opts_new`
