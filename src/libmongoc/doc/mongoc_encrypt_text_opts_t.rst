:man_page: mongoc_encrypt_text_opts_t

mongoc_encrypt_text_opts_t
=============================================

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_encrypt_text_opts_t mongoc_encrypt_text_opts_t;

.. versionadded:: 2.2.0

TextOpts specifies index options for a Queryable Encryption field supporting "textPreview" queries. Used to set options for :symbol:`mongoc_client_encryption_encrypt()`.

Case sensitive and diacritic sensitive must be set. If prefix or suffix are set, substring must not be set.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_encrypt_text_opts_new
    mongoc_encrypt_text_opts_destroy
    mongoc_encrypt_text_opts_set_case_sensitive
    mongoc_encrypt_text_opts_set_diacritic_sensitive
    mongoc_encrypt_text_opts_set_prefix
    mongoc_encrypt_text_opts_set_suffix
    mongoc_encrypt_text_opts_set_substring
    mongoc_encrypt_text_prefix_opts_new
    mongoc_encrypt_text_prefix_opts_destroy
    mongoc_encrypt_text_prefix_opts_set_str_max_query_length
    mongoc_encrypt_text_prefix_opts_set_str_min_query_length
    mongoc_encrypt_text_suffix_opts_new
    mongoc_encrypt_text_suffix_opts_destroy
    mongoc_encrypt_text_suffix_opts_set_str_max_query_length
    mongoc_encrypt_text_suffix_opts_set_str_min_query_length
    mongoc_encrypt_text_substring_opts_new
    mongoc_encrypt_text_substring_opts_destroy
    mongoc_encrypt_text_substring_opts_set_str_max_length
    mongoc_encrypt_text_substring_opts_set_str_max_query_length
    mongoc_encrypt_text_substring_opts_set_str_min_query_length
    mongoc_client_encryption_encrypt_opts_set_text_opts

.. seealso::

  | :symbol:`mongoc_client_encryption_encrypt()`
  | :symbol:`mongoc_client_encryption_encrypt_opts_t()`
