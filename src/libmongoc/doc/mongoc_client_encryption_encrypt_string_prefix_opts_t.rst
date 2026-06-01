:man_page: mongoc_client_encryption_encrypt_string_prefix_opts_t

mongoc_client_encryption_encrypt_string_prefix_opts_t
===================================================

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_client_encryption_encrypt_string_prefix_opts_t mongoc_client_encryption_encrypt_string_prefix_opts_t;

.. versionchanged:: 2.4.0

   Renamed from the previously experimental ``encrypt_text_*`` API. This is a backwards-incompatible change.

StringPrefixOpts specifies options for a Queryable Encryption field supporting "prefix" queries. Used to set options for :symbol:`mongoc_client_encryption_encrypt_string_opts_set_prefix()`.


.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_client_encryption_encrypt_string_prefix_opts_new
    mongoc_client_encryption_encrypt_string_prefix_opts_set_str_max_query_length
    mongoc_client_encryption_encrypt_string_prefix_opts_set_str_min_query_length

.. seealso::

  | :symbol:`mongoc_client_encryption_encrypt()`
  | :symbol:`mongoc_client_encryption_encrypt_opts_t()`
