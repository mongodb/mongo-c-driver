:man_page: mongoc_client_encryption_encrypt_text_substring_opts_set_str_min_query_length

mongoc_client_encryption_encrypt_text_substring_opts_set_str_min_query_length()
===============================================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_text_substring_opts_set_str_min_query_length(
      mongoc_client_encryption_encrypt_text_substring_opts_t *opts,
      int32_t str_min_query_length);

.. versionadded:: 2.2.0

Sets the minimum query length for substring text search encryption options.

|encrypt-text-is-experimental|

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_text_substring_opts_t`.
* ``str_min_query_length``: The minimum query length for substring search. Must be greater than zero.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_text_substring_opts_new`
   | :symbol:`mongoc_client_encryption_encrypt_text_opts_set_substring`
