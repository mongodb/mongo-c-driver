:man_page: mongoc_client_encryption_encrypt_text_opts_new

mongoc_client_encryption_encrypt_text_opts_new()
==============================

Synopsis
--------

.. code-block:: c

   mongoc_client_encryption_encrypt_text_opts_t *
   mongoc_client_encryption_encrypt_text_opts_new(void);

.. versionadded:: 2.2.0

|encrypt-text-is-experimental|

Returns
-------

A new :symbol:`mongoc_client_encryption_encrypt_text_opts_t` that must be freed with :symbol:`mongoc_client_encryption_encrypt_text_opts_destroy()`.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_text_opts_destroy`
   | :symbol:`mongoc_client_encryption_encrypt_text_opts_set_prefix`
   | :symbol:`mongoc_client_encryption_encrypt_text_opts_set_suffix`
   | :symbol:`mongoc_client_encryption_encrypt_text_opts_set_substring`
