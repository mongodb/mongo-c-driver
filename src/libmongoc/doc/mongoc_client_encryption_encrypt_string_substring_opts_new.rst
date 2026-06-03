:man_page: mongoc_client_encryption_encrypt_string_substring_opts_new

mongoc_client_encryption_encrypt_string_substring_opts_new()
============================================================

Synopsis
--------

.. code-block:: c

   mongoc_client_encryption_encrypt_string_substring_opts_t *
   mongoc_client_encryption_encrypt_string_substring_opts_new(void);

.. versionchanged:: 2.4.0

   Renamed from the previously experimental ``encrypt_text_*`` API. This is a backwards-incompatible change.

Returns
-------

A new :symbol:`mongoc_client_encryption_encrypt_string_substring_opts_t` that must be freed with :symbol:`mongoc_client_encryption_encrypt_string_substring_opts_destroy()`.

|encrypt-string-substring-is-experimental|

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_string_substring_opts_destroy`
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_set_substring`
