:man_page: mongoc_client_encryption_encrypt_text_substring_opts_t

mongoc_client_encryption_encrypt_text_substring_opts_t
======================================================

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_client_encryption_encrypt_text_substring_opts_t mongoc_client_encryption_encrypt_text_substring_opts_t;

.. versionadded:: 2.2.0

TextSubstringOpts specifies options for a Queryable Encryption field supporting "substringPreview" queries. Used to set options for :symbol:`mongoc_client_encryption_encrypt_text_opts_set_substring()`.

|encrypt-text-is-experimental|

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_client_encryption_encrypt_text_substring_opts_new
    mongoc_client_encryption_encrypt_text_substring_opts_set_str_max_query_length
    mongoc_client_encryption_encrypt_text_substring_opts_set_str_min_query_length
    mongoc_client_encryption_encrypt_text_substring_opts_set_str_max_length

.. seealso::

  | :symbol:`mongoc_client_encryption_encrypt()`
  | :symbol:`mongoc_client_encryption_encrypt_opts_t()`
