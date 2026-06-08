:man_page: mongoc_client_encryption_encrypt_string_opts_set_case_sensitive

mongoc_client_encryption_encrypt_string_opts_set_case_sensitive()
=================================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_string_opts_set_case_sensitive(
      mongoc_client_encryption_encrypt_string_opts_t *opts,
      bool case_sensitive);

.. versionchanged:: 2.4.0

   Renamed from the now-deprecated ``text`` API.

Sets whether string search is case sensitive.


Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_string_opts_t`.
* ``case_sensitive``: If true, string search is case sensitive.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_t`
