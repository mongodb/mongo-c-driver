:man_page: mongoc_client_encryption_encrypt_text_opts_set_suffix

mongoc_client_encryption_encrypt_text_opts_set_suffix()
=======================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_text_opts_set_suffix(
      mongoc_client_encryption_encrypt_text_opts_t *opts,
      mongoc_client_encryption_encrypt_text_suffix_opts_t *sopts);

.. versionadded:: 2.2.0

Sets the suffix options for text search encryption.

|encrypt-text-is-experimental|

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_text_opts_t`.
* ``sopts``: A :symbol:`mongoc_client_encryption_encrypt_text_suffix_opts_t` to set as suffix options.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_text_opts_new`
   | :symbol:`mongoc_client_encryption_encrypt_text_suffix_opts_new`
