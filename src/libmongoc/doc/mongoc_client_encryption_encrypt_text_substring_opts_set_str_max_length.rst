:man_page: mongoc_client_encryption_encrypt_text_substring_opts_set_str_max_length

mongoc_client_encryption_encrypt_text_substring_opts_set_str_max_length()
=========================================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_text_substring_opts_set_str_max_length(
      mongoc_client_encryption_encrypt_text_substring_opts_t *opts,
      int32_t str_max_length);

.. versionadded:: 2.2.0

Sets the maximum string length for substring text search encryption options.

|encrypt-text-is-experimental|

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_text_substring_opts_t`.
* ``str_max_length``: The maximum string length for substring search. Must be greater than zero.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_text_substring_opts_new`
   | :symbol:`mongoc_client_encryption_encrypt_text_opts_set_substring`
