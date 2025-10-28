:man_page: mongoc_client_encryption_encrypt_text_opts_set_case_sensitive

mongoc_client_encryption_encrypt_text_opts_set_case_sensitive()
=============================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_text_opts_set_case_sensitive(
      mongoc_client_encryption_encrypt_text_opts_t *opts,
      bool case_sensitive);

.. versionadded:: 2.2.0

Sets whether text search is case sensitive.

|encrypt-text-is-experimental|

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_text_opts_t`.
* ``case_sensitive``: If true, text search is case sensitive.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_text_opts_t`
