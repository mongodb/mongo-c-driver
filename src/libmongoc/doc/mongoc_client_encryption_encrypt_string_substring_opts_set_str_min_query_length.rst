:man_page: mongoc_client_encryption_encrypt_string_substring_opts_set_str_min_query_length

mongoc_client_encryption_encrypt_string_substring_opts_set_str_min_query_length()
=================================================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_string_substring_opts_set_str_min_query_length(
      mongoc_client_encryption_encrypt_string_substring_opts_t *opts,
      int32_t str_min_query_length);

.. versionchanged:: 2.4.0

   Renamed from the previously experimental ``encrypt_text_*`` API. This is a backwards-incompatible change.

Sets the minimum query length for substring text search encryption options.


Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_string_substring_opts_t`.
* ``str_min_query_length``: The minimum query length for substring search. Must be greater than zero.

|encrypt-string-substring-is-experimental|

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_string_substring_opts_new`
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_set_substring`
