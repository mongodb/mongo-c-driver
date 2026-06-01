:man_page: mongoc_client_encryption_encrypt_string_opts_t

mongoc_client_encryption_encrypt_string_opts_t
=============================================

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_client_encryption_encrypt_string_opts_t mongoc_client_encryption_encrypt_string_opts_t;

.. versionchanged:: 2.4.0

   Renamed from the previously experimental ``encrypt_text_*`` API. This is a backwards-incompatible change.

StringOpts specifies index options for a Queryable Encryption field supporting "textPreview" queries. Used to set options for :symbol:`mongoc_client_encryption_encrypt()`.

Case sensitive and diacritic sensitive must be set. If prefix or suffix are set, substring must not be set.


.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_client_encryption_encrypt_string_opts_new
    mongoc_client_encryption_encrypt_string_opts_destroy
    mongoc_client_encryption_encrypt_string_opts_set_case_sensitive
    mongoc_client_encryption_encrypt_string_opts_set_diacritic_sensitive
    mongoc_client_encryption_encrypt_string_opts_set_prefix
    mongoc_client_encryption_encrypt_string_opts_set_suffix
    mongoc_client_encryption_encrypt_string_opts_set_substring
    mongoc_client_encryption_encrypt_string_prefix_opts_t
    mongoc_client_encryption_encrypt_string_prefix_opts_new
    mongoc_client_encryption_encrypt_string_prefix_opts_destroy
    mongoc_client_encryption_encrypt_string_prefix_opts_set_str_max_query_length
    mongoc_client_encryption_encrypt_string_prefix_opts_set_str_min_query_length
    mongoc_client_encryption_encrypt_string_suffix_opts_t
    mongoc_client_encryption_encrypt_string_suffix_opts_new
    mongoc_client_encryption_encrypt_string_suffix_opts_destroy
    mongoc_client_encryption_encrypt_string_suffix_opts_set_str_max_query_length
    mongoc_client_encryption_encrypt_string_suffix_opts_set_str_min_query_length
    mongoc_client_encryption_encrypt_string_substring_opts_t
    mongoc_client_encryption_encrypt_string_substring_opts_new
    mongoc_client_encryption_encrypt_string_substring_opts_destroy
    mongoc_client_encryption_encrypt_string_substring_opts_set_str_max_length
    mongoc_client_encryption_encrypt_string_substring_opts_set_str_max_query_length
    mongoc_client_encryption_encrypt_string_substring_opts_set_str_min_query_length
    mongoc_client_encryption_encrypt_opts_set_string_opts

.. seealso::

  | :symbol:`mongoc_client_encryption_encrypt()`
  | :symbol:`mongoc_client_encryption_encrypt_opts_t()`
