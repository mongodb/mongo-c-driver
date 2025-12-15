:man_page: mongoc_client_encryption_encrypt_text_prefix_opts_destroy

mongoc_client_encryption_encrypt_text_prefix_opts_destroy()
===========================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_text_prefix_opts_destroy(mongoc_client_encryption_encrypt_text_prefix_opts_t *opts);

.. versionadded:: 2.2.0

Frees a :symbol:`mongoc_client_encryption_encrypt_text_prefix_opts_t` created with :symbol:`mongoc_client_encryption_encrypt_text_prefix_opts_new()`.

|encrypt-text-is-experimental|

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_text_prefix_opts_t` to destroy.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_text_prefix_opts_new`
