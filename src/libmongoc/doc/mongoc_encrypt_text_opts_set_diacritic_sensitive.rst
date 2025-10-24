:man_page: mongoc_encrypt_text_opts_set_diacritic_sensitive

mongoc_encrypt_text_opts_set_diacritic_sensitive()
==================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_encrypt_text_opts_set_diacritic_sensitive(
      mongoc_encrypt_text_opts_t *opts,
      bool diacritic_sensitive);

.. versionadded:: 2.2.0

Sets whether text search is diacritic sensitive.

Parameters
----------

* ``opts``: A :symbol:`mongoc_encrypt_text_opts_t`.
* ``diacritic_sensitive``: If true, text search is diacritic sensitive.

.. seealso::
   | :symbol:`mongoc_encrypt_text_opts_t`
