:man_page: mongoc_client_encryption_encrypt_string_opts_destroy

mongoc_client_encryption_encrypt_string_opts_destroy()
======================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_string_opts_destroy(mongoc_client_encryption_encrypt_string_opts_t *topts);

.. versionchanged:: 2.4.0

   Renamed from the previously experimental ``encrypt_text_*`` API. This is a backwards-incompatible change.

Frees a :symbol:`mongoc_client_encryption_encrypt_string_opts_t` created with :symbol:`mongoc_client_encryption_encrypt_string_opts_new()`.


Parameters
----------

* ``topts``: A :symbol:`mongoc_client_encryption_encrypt_string_opts_t` to destroy.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_new`
