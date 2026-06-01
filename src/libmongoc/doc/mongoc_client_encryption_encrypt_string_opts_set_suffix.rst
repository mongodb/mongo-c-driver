:man_page: mongoc_client_encryption_encrypt_string_opts_set_suffix

mongoc_client_encryption_encrypt_string_opts_set_suffix()
=======================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_string_opts_set_suffix(
      mongoc_client_encryption_encrypt_string_opts_t *opts,
      mongoc_client_encryption_encrypt_string_suffix_opts_t *sopts);

.. versionchanged:: 2.4.0

   Renamed from the previously experimental ``encrypt_text_*`` API. This is a backwards-incompatible change.

Sets the suffix options for text search encryption.


Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_string_opts_t`.
* ``sopts``: A :symbol:`mongoc_client_encryption_encrypt_string_suffix_opts_t` to set as suffix options.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_new`
   | :symbol:`mongoc_client_encryption_encrypt_string_suffix_opts_new`
