:man_page: mongoc_client_encryption_encrypt_string_opts_set_substring

mongoc_client_encryption_encrypt_string_opts_set_substring()
============================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_string_opts_set_substring(
      mongoc_client_encryption_encrypt_string_opts_t *opts,
      mongoc_client_encryption_encrypt_string_substring_opts_t *ssopts);

.. versionchanged:: 2.4.0

   Renamed from the previously experimental ``encrypt_text_*`` API. This is a backwards-incompatible change.

Sets the substring options for text search encryption.


Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_string_opts_t`.
* ``ssopts``: A :symbol:`mongoc_client_encryption_encrypt_string_substring_opts_t` to set as substring options.

|encrypt-string-substring-is-experimental|

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_new`
   | :symbol:`mongoc_client_encryption_encrypt_string_substring_opts_new`
