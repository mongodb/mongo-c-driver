:man_page: mongoc_client_encryption_encrypt_string_suffix_opts_destroy

mongoc_client_encryption_encrypt_string_suffix_opts_destroy()
=============================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_string_suffix_opts_destroy(mongoc_client_encryption_encrypt_string_suffix_opts_t *opts);

.. versionchanged:: 2.4.0

   Renamed from the now-deprecated ``text`` API.

Frees a :symbol:`mongoc_client_encryption_encrypt_string_suffix_opts_t` created with :symbol:`mongoc_client_encryption_encrypt_string_suffix_opts_new()`.


Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_string_suffix_opts_t` to destroy.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_string_suffix_opts_new`
