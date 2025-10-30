:man_page: mongoc_client_encryption_encrypt_text_opts_destroy

mongoc_client_encryption_encrypt_text_opts_destroy()
==================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_text_opts_destroy(mongoc_client_encryption_encrypt_text_opts_t *topts);

.. versionadded:: 2.2.0

Frees a :symbol:`mongoc_client_encryption_encrypt_text_opts_t` created with :symbol:`mongoc_client_encryption_encrypt_text_opts_new()`.

|encrypt-text-is-experimental|

Parameters
----------

* ``topts``: A :symbol:`mongoc_client_encryption_encrypt_text_opts_t` to destroy.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_text_opts_new`
