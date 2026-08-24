:man_page: mongoc_client_encryption_encrypt_string_opts_set_diacritic_sensitive

mongoc_client_encryption_encrypt_string_opts_set_diacritic_sensitive()
======================================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_string_opts_set_diacritic_sensitive(
      mongoc_client_encryption_encrypt_string_opts_t *opts,
      bool diacritic_sensitive);

.. versionchanged:: 2.4.0

   Renamed from the now-deprecated ``text`` API.

Sets whether string search is diacritic sensitive.


Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_string_opts_t`.
* ``diacritic_sensitive``: If true, string search is diacritic sensitive.

.. seealso::
   | :symbol:`mongoc_client_encryption_encrypt_string_opts_t`
