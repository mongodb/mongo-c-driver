:man_page: mongoc_client_encryption_encrypt_string_opts_set_prefix

mongoc_client_encryption_encrypt_string_opts_set_prefix()
=========================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_string_opts_set_prefix(
      mongoc_client_encryption_encrypt_string_opts_t *opts,
      mongoc_client_encryption_encrypt_string_prefix_opts_t *popts);

.. versionchanged:: 2.4.0

   Renamed from the now-deprecated ``text`` API.

Sets the prefix options for string search encryption.


Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_string_opts_t`.
* ``popts``: A :symbol:`mongoc_client_encryption_encrypt_string_prefix_opts_t` to set as prefix options.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_new`
   | :symbol:`mongoc_client_encryption_encrypt_string_prefix_opts_new`
