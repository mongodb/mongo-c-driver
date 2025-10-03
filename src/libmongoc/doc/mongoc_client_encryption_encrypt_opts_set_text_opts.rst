:man_page: mongoc_client_encryption_encrypt_opts_set_text_opts

mongoc_client_encryption_encrypt_opts_set_text_opts()
====================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_opts_set_text_opts(
      mongoc_client_encryption_encrypt_opts_t *opts,
      const mongoc_encrypt_text_opts_t *text_opts);

.. versionadded:: 2.2.0

Sets the text search encryption options for explicit encryption.

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_opts_t`.
* ``text_opts``: A :symbol:`mongoc_encrypt_text_opts_t` specifying text search options.

.. seealso::
   | :symbol:`mongoc_encrypt_text_opts_t`
   | :symbol:`mongoc_encrypt_text_opts_new`
   | :symbol:`mongoc_encrypt_text_opts_set_prefix`
   | :symbol:`mongoc_encrypt_text_opts_set_suffix`
   | :symbol:`mongoc_encrypt_text_opts_set_substring`
