:man_page: mongoc_client_encryption_encrypt_opts_set_string_opts

mongoc_client_encryption_encrypt_opts_set_string_opts()
=======================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_opts_set_string_opts(
      mongoc_client_encryption_encrypt_opts_t *opts,
      const mongoc_client_encryption_encrypt_string_opts_t *string_opts);

.. versionchanged:: 2.4.0

   Renamed from the previously experimental ``encrypt_text_*`` API. This is a backwards-incompatible change.

Sets the text search encryption options for explicit encryption.


Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_opts_t`.
* ``string_opts``: A :symbol:`mongoc_client_encryption_encrypt_string_opts_t` specifying string search options.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_t`
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_new`
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_set_prefix`
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_set_suffix`
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_set_substring`
